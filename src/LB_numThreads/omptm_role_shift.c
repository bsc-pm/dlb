/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#include "LB_numThreads/omptm_role_shift.h"

#include "apis/dlb.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_core/spd.h"

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

/* OpenMP symbols */
int __kmp_get_num_threads_role(ompt_role_t r) __attribute__((weak));
int __kmp_get_thread_roles(int tid, ompt_role_t *r) __attribute((weak));
void __kmp_set_thread_roles1(int how_many, ompt_role_t r) __attribute((weak));
void __kmp_set_thread_roles2(int tid, ompt_role_t r) __attribute((weak));
int __kmp_get_thread_id(void) __attribute((weak));

void Extrae_change_num_threads (unsigned) __attribute__((weak));
void Extrae_set_threadid_function (unsigned (*)(void)) __attribute__((weak));
//int __kmp_get_free_agent_id(void) __attribute__((weak));
//int __kmp_get_num_free_agent_threads(void) __attribute__((weak));
//void __kmp_set_free_agent_thread_active_status(
//        unsigned int thread_num, bool active) __attribute__((weak));

/* Enum for ompt_data_t *parallel_data to detect level 1 (non nested) parallel
 * regions */
enum {
    PARALLEL_UNSET,
    PARALLEL_LEVEL_1,
};


/*** Static variables ***/

static bool lewi = false;
static pid_t pid;
static omptool_opts_t omptool_opts;
static int primary_thread_cpu;
static int system_size;
static int default_num_threads;
static int num_free_agents = 0;

/* Masks */
static cpu_set_t active_mask;
static cpu_set_t process_mask;
static cpu_set_t primary_thread_mask;
//static cpu_set_t worker_threads_mask;

/* Atomic variables */
static atomic_bool DLB_ALIGN_CACHE in_parallel = false;
static atomic_int  DLB_ALIGN_CACHE current_parallel_size = 0;
static atomic_uint DLB_ALIGN_CACHE pending_tasks = 0;

static pthread_mutex_t mutex_num_fa = PTHREAD_MUTEX_INITIALIZER;

/* Thread local */
__thread int global_tid = -1; //References to the thread id of the kmp runtime

/*********************************************************************************/
/*  CPU Data structures and helper atomic flags functions                        */
/*********************************************************************************/

/* Current state of the CPU (what is being used for) */
typedef enum CPUStatus {
    OWN      = 0,
    UNKNOWN  = 1 << 0,
    LENT     = 1 << 1,
    BORROWED = 1 << 2
} cpu_status_t;

/* Possible OpenMP roles that the CPU can take */
typedef enum OpenMP_Roles {
    ROLE_NONE       = 0,
    ROLE_PRIMARY    = 1 << 0,
    ROLE_WORKER     = 1 << 1,
    ROLE_FREE_AGENT = 1 << 2,
} openmp_roles_t;

typedef struct DLB_ALIGN_CACHE CPU_Data {
    cpu_status_t ownership;
    atomic_bool  free_cpu; //True: Unused, False: in use
    bool          fa; 
} cpu_data_t;


static atomic_int registered_threads = 0; 
static cpu_data_t *cpu_data = NULL;
static int *cpu_by_id = NULL;

static unsigned int get_thread_id(void){
    unsigned int a = (unsigned int)__kmp_get_thread_id();
    //printf("Executing get thread id for thread %d \n", a);
    return a;
}

static int get_id_from_cpu(int cpuid){
    int i;
    for(i = 0; i < registered_threads; i++){
        if(cpu_by_id[i] == cpuid)
            return i;
    }
    return -1;
}

/*********************************************************************************/
/*  DLB callbacks                                                                */
/*********************************************************************************/

static void cb_enable_cpu(int cpuid, void *arg) {
    warning("Primary thread %d, enabling cpu: %d", primary_thread_cpu, cpuid);
    
    int pos = get_id_from_cpu(cpuid);
    if(cpu_data[cpuid].ownership == LENT){ 
        //We are reclaiming a previousley LENT cpu
        cpu_data[cpuid].ownership = OWN;
    }
    else if(cpu_data[cpuid].ownership == UNKNOWN){
        cpu_data[cpuid].ownership = BORROWED;
        //cpu_data[cpuid].free_cpu = true;
    }
    
    
    if(pos != -1){ //A thread was running here previously
        warning("Primary thread %d, CPU ID: %d, Is FA? %d", primary_thread_cpu, cpuid, cpu_data[cpuid].fa);
        if(!cpu_data[cpuid].fa){ //CPU from a worker, better not to touch it
            cpu_data[cpuid].free_cpu = false;
        }
        else{//We had a FA here. Call the api to wake it up and mark the CPU as free here
            cpu_data[cpuid].free_cpu = true;
            cpu_data[cpuid].fa = false;
            pthread_mutex_lock(&mutex_num_fa);
            ++num_free_agents;
            __kmp_set_thread_roles2(pos, OMP_ROLE_FREE_AGENT);
            pthread_mutex_unlock(&mutex_num_fa);
        }
    }
    else{//ask for a new FA
        cpu_data[cpuid].free_cpu = true;
        cpu_data[cpuid].fa = false;
        pthread_mutex_lock(&mutex_num_fa);
        if(DLB_ATOMIC_LD_RLX(&in_parallel)){
            if(num_free_agents < DLB_ATOMIC_LD_RLX(&current_parallel_size)){
                num_free_agents = DLB_ATOMIC_LD_RLX(&current_parallel_size);
            }
        }
        ++num_free_agents;
        __kmp_set_thread_roles1(num_free_agents, OMP_ROLE_FREE_AGENT);
        pthread_mutex_unlock(&mutex_num_fa);
    }
}

static void cb_disable_cpu(int cpuid, void *arg) {
	if(cpu_data[cpuid].fa){
	    //cpu_data[cpuid].fa = false;
        pthread_mutex_lock(&mutex_num_fa);
        --num_free_agents;
        __kmp_set_thread_roles2(global_tid, OMP_ROLE_NONE);
        pthread_mutex_unlock(&mutex_num_fa);
	}
	if(cpu_data[cpuid].ownership == BORROWED)
	    cpu_data[cpuid].ownership = UNKNOWN;
	else if(cpu_data[cpuid].ownership == OWN)
	    cpu_data[cpuid].ownership = LENT;
	//cpu_data[cpuid].free_cpu = false;
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
}


/* Look for local available CPUs, if none is found ask LeWI */
/*static void acquire_one_thread(void) {
	if(lewi){
		if(DLB_AcquireCpus(1) == DLB_SUCCESS){
			if(blocked_threads){ //Wake up one of the threads waiting on a cond variable
                
			}
			else{ //Ask the runtime for a new free agent
			    pthread_mutex_lock(&mutex_num_fa);
			    //int fa = DLB_ATOMIC_LD_RLX(&num_free_agents);
			    printf("Primary cpu %d increasing the number of free agents from %d to %d \n", primary_thread_cpu, num_free_agents, num_free_agents+1);
			    ++num_free_agents;
			    __kmp_set_thread_roles1(num_free_agents, OMP_ROLE_FREE_AGENT);
			    //DLB_ATOMIC_ST(&num_free_agents, fa);
			    pthread_mutex_unlock(&mutex_num_fa);
		    }
		}
	}
}*/

/* Obtain a CPU id for a given free agent id */
//static int get_free_agent_binding(int thread_id) {
//}


/*********************************************************************************/
/*  Init & Finalize module                                                       */
/*********************************************************************************/

void omptm_role_shift__init(pid_t process_id, const options_t *options) {

    /* Initialize static variables */
    system_size = mu_get_system_size();
    lewi = options->lewi;
    omptool_opts = options->lewi_ompt;
    pid = process_id;
    num_free_agents = __kmp_get_num_threads_role(OMP_ROLE_FREE_AGENT);
    shmem_procinfo__getprocessmask(pid, &process_mask, 0);
    verbose(VB_OMPT, "Process mask: %s", mu_to_str(&process_mask));

    // omp_get_max_threads cannot be called here, try using the env. var.
    const char *env_omp_num_threads = getenv("OMP_NUM_THREADS");
    default_num_threads = env_omp_num_threads 
    											? atoi(env_omp_num_threads)
                          : CPU_COUNT(&process_mask);
    cpu_data = malloc(sizeof(cpu_data_t)*system_size);
    cpu_by_id = malloc(sizeof(int)*system_size);
    
    CPU_ZERO(&primary_thread_mask);
    int num_workers = default_num_threads - num_free_agents;
    int encountered_cpus = 0;
    int i;
    for(i = 0; i < system_size; i++){
        cpu_by_id[i] = -1;
    }
    //Building of the cpu_data structure. It holds info of the different CPUs from the node of the process
    for(i = 0; i < system_size; i++){
    	if(CPU_ISSET(i, &process_mask)){
    		if(++encountered_cpus == 1){
    			//First encountered CPU belongs to the primary thread
				cpu_data[i].free_cpu = false;
				primary_thread_cpu = i;
				CPU_SET(i, &primary_thread_mask);
				cpu_by_id[encountered_cpus - 1] = i;
			}
			else if(encountered_cpus <= num_workers){
				//Assume the next CPUs after the primary thread are for the workers and are not free.
				cpu_data[i].free_cpu = false;
				cpu_by_id[encountered_cpus - 1] = i;
			}
			else{
				//Don't assume a CPU will be used until starting some thread in it.
			    cpu_data[i].free_cpu = true;
			}
			cpu_data[i].ownership = OWN;
		}
		else{
			cpu_data[i].ownership = UNKNOWN;
			cpu_data[i].free_cpu = true;
		}
		cpu_data[i].fa = false;
	}
	memcpy(&active_mask, &primary_thread_mask, sizeof(cpu_set_t));

	//Extrae functions configuration
	if(Extrae_change_num_threads){
	    Extrae_change_num_threads(20);
	    Extrae_set_threadid_function(&get_thread_id);
	}
    
    if (lewi) {
        int err;
        err = DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet enable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet disable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_set_process_mask,
                (dlb_callback_t)cb_set_process_mask, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet set_process_mask: %s", DLB_Strerror(err));
        }

        //omptm_role_shift__lend();
    }
}

void omptm_role_shift__finalize(void) {
	free(cpu_data);
	cpu_data = NULL;
	pthread_mutex_destroy(&mutex_num_fa);
}


/*********************************************************************************/
/*  Blocking calls specific functions                                           */
/*********************************************************************************/

/*TODO: what happens when executing in "ompss" mode*/
void omptm_role_shift__IntoBlockingCall(void) {
    if (lewi && omptool_opts & OMPTOOL_OPTS_MPI) {

        /* Don't know what to do if a Blocking Call is invoked inside a
         * parallel region. We could ignore it, but then we should also ignore
         * the associated OutOfBlockingCall, and how would we know it? */
         //TODO: Do we need the in parallel flag now?
        fatal_cond(DLB_ATOMIC_LD(&in_parallel),
                "Blocking Call inside a parallel region not supported");
        //TODO: Aixo crida al cb_disable al final? Hi ha afectacio als cycles per us??
        cpu_set_t cpus_to_lend;
        CPU_ZERO(&cpus_to_lend);
        int i;
        for(i = 0; i < system_size; i++){
            if(cpu_data[i].ownership == OWN){
                //cpu_data[i].free_cpu = false;
                cpu_data[i].ownership = LENT;
                CPU_SET(i, &cpus_to_lend);
            }
            else if(cpu_data[i].ownership == BORROWED){
                cpu_data[i].ownership = UNKNOWN;
                CPU_SET(i, &cpus_to_lend);
            }
        }
        DLB_LendCpuMask(&cpus_to_lend);
        warning("Primary thread %d, lending cpus: %s", primary_thread_cpu, mu_to_str(&cpus_to_lend));
        //DLB_PrintShmem(0,0);

        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}


void omptm_role_shift__OutOfBlockingCall(void) {
    if (lewi) {
        if (omptool_opts & OMPTOOL_OPTS_LEND) {
            /* Do nothing.
             * Do not reclaim since going out of a blocking call is not
             * an indication that the CPUs may be needed. */
        }
        else if (omptool_opts & OMPTOOL_OPTS_MPI) {
            DLB_PrintShmem(0,0);
            cb_enable_cpu(cpu_by_id[global_tid], NULL);
            DLB_Reclaim();
            warning("Primary thread %d, reclaiming all cpus", primary_thread_cpu);
        }
    }
}


/*********************************************************************************/
/*  OMPT registered callbacks                                                    */
/*********************************************************************************/

void omptm_role_shift__thread_begin(
				ompt_thread_t thread_type,
				ompt_data_t *thread_data){
	/* Set up thread local spd */
	spd_enter_dlb(NULL);
	global_tid = __kmp_get_thread_id();
	
	DLB_ATOMIC_ADD(&registered_threads, 1);
	
	if(thread_type == ompt_thread_other){ //other => free agent
        cpu_set_t thread_mask;
        int cpuid;
        for(cpuid = 0; cpuid < system_size; cpuid++){
            if((cpu_data[cpuid].ownership == OWN ||
                cpu_data[cpuid].ownership == BORROWED) &&
                cpu_data[cpuid].free_cpu){
                
                cpu_data[cpuid].free_cpu = false;
                cpu_data[cpuid].fa = true;
                break;
            }
        }
        if(cpuid < system_size){//We found an available CPU
            cpu_by_id[global_tid] = cpuid;
            CPU_ZERO(&thread_mask);
            CPU_SET(cpuid, &thread_mask);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
            instrument_event(REBIND_EVENT, cpuid+1, EVENT_BEGIN);
        }
        else{ //Not likely, but we didn't find a CPU for a new FA
            cpu_by_id[global_tid] = -2; //We'll try to reschedule in a task schedule point
        }
	}
	//TODO: Bind the free agents to available CPUs but not the workers?? What to do 
	/* thread_type == ompt_thread_other  ==> free agent
	 * thread_type == ompt_thread_worker ==> normal thread*/
}

/* A thread has transitioned from prior_role to next_role. If prior_role was OMP_ROLE_FREE_AGENT,
 * DLB should untrack that thread and potentially Return/Lend the CPU. If next_role is
 * OMP_ROLE_FREE_AGENT, DLB must register that and bind it to the appropriate CPU */
void omptm_role_shift__thread_role_shift(
				ompt_data_t *thread_data,
				ompt_role_t prior_role,
				ompt_role_t next_role){
	if(prior_role == OMP_ROLE_FREE_AGENT){
		if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
		verbose(VB_OMPT, "Free agent %d changing the role to NONE", global_tid);
	}
	else if(prior_role == OMP_ROLE_NONE){
		if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
        warning("Primary thread %d, thread %d shifting to FA", primary_thread_cpu, global_tid);
		cpu_set_t thread_mask;
		int cpuid;
		if(cpu_by_id[global_tid] >= 0){
		    //The thread had a CPU previously. Let's stick to that CPU
		    cpuid = cpu_by_id[global_tid];
		}
		else{ //If not, search for a suitable one
    		for(cpuid = 0; cpuid < system_size; cpuid++){
	    		if((cpu_data[cpuid].ownership == OWN || cpu_data[cpuid].ownership == BORROWED) &&
    		    	    cpu_data[cpuid].free_cpu){
			        break;	
			    }
			}
		}
		if(cpuid < system_size){
		    cpu_data[cpuid].free_cpu = false;
		    cpu_data[cpuid].fa = true;
		    cpu_by_id[global_tid] = cpuid;
		    CPU_ZERO(&thread_mask);
	    	CPU_SET(cpuid, &thread_mask);
    		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
    		instrument_event(BINDINGS_EVENT, cpuid+1, EVENT_BEGIN);
		    verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
		}
		else{//We didn't found a suitable CPU for that thread. Mark to reschedule as soon as possible
		    cpu_by_id[global_tid] = -2;
		}
	}
}


void omptm_role_shift__parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_parallelism,
        int flags,
        const void *codeptr_ra) {
    /* From OpenMP spec:
     * "The exit frame associated with the initial task that is not nested
     *  inside any OpenMP construct is NULL."
     */
    if (encountering_task_frame->exit_frame.ptr == NULL
            && flags & ompt_parallel_team) {

        /* This is a non-nested parallel construct encountered by the initial task.
         * Set parallel_data to an appropriate value so that worker threads know
         * when they start their explicit task for this parallel region.
         */
        parallel_data->value = PARALLEL_LEVEL_1;
        DLB_ATOMIC_ST(&in_parallel, true);
        DLB_ATOMIC_ST(&current_parallel_size, requested_parallelism);
        //DLB_ATOMIC_ADD(&num_free_agents, requested_parallelism);
        //pthread_mutex_lock(&mutex_num_fa);
        //DLB_ATOMIC_ST(&fa_before_parallel, num_free_agents);
		//printf("Primary cpu %d increasing the number of free agents from %d to %d \n", primary_thread_cpu, num_free_agents, num_free_agents+requested_parallelism);
        //num_free_agents += requested_parallelism;
        //pthread_mutex_unlock(&mutex_num_fa);
    }
}

void omptm_role_shift__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (parallel_data->value == PARALLEL_LEVEL_1) {
        parallel_data->value = PARALLEL_UNSET;
		//printf("Primary cpu %d decreasing the number of free agents from %d to %d \n", primary_thread_cpu, num_free_agents, num_free_agents-parallel_team_size);
        //DLB_ATOMIC_SUB(&num_free_agents, DLB_ATOMIC_LD(&parallel_team_size));
        //TODO:Maybe the next store isn't needed
        //DLB_ATOMIC_ST(&parallel_team_size, 0);
        DLB_ATOMIC_ST(&in_parallel, false);
				//TODO: Lend aqui???
        //omptm_role_shift__lend();
    }
}

void omptm_role_shift__task_create(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *new_task_data,
        int flags,
        int has_dependences,
        const void *codeptr_ra) {
    if (flags & ompt_task_explicit) {
        /* Increment the amount of pending tasks */
        DLB_ATOMIC_ADD(&pending_tasks, 1);

        /* For now, let's assume that we always want to increase the number
         * of active threads whenever a task is created
         */
        DLB_AcquireCpus(1);
        //acquire_one_thread();
    }
}

void omptm_role_shift__task_schedule(
        ompt_data_t *prior_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *next_task_data) {
    if (prior_task_status == ompt_task_switch) {
        if (DLB_ATOMIC_SUB(&pending_tasks, 1) > 1) {
            DLB_AcquireCpus(1);
            //acquire_one_thread();
        }
        if(cpu_by_id[global_tid] == -2){ 
            //We couldn't find a free for this thread previously. Try to rebind it now
            int i;
            for(i = 0; i < system_size; i++){
                if((cpu_data[i].ownership == OWN || cpu_data[i].ownership == BORROWED) &&
                        cpu_data[i].free_cpu){
                    cpu_data[i].free_cpu = false;
                    cpu_data[i].fa = true;
                    break;
                }
            }
            if(i < system_size){
                cpu_by_id[global_tid] = i;
                cpu_set_t thread_mask;
                CPU_ZERO(&thread_mask);
                CPU_SET(i, &thread_mask);
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
                verbose(VB_OMPT, "Binding a free agent to CPU %d", i);
                instrument_event(REBIND_EVENT, i+1, EVENT_BEGIN);
            }
        }
        instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
    } else if (prior_task_status == ompt_task_complete) {
        int cpuid = cpu_by_id[global_tid];
        if (cpuid >= 0 && cpu_data[cpuid].fa) {
            /* Return CPU if reclaimed */
            if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
                warning("Primary thread %d, disabling and returning cpu: %d", primary_thread_cpu, cpuid);
                if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                    cb_disable_cpu(cpuid, NULL);
                    //warning("Primary thread %d, disabling and returning cpu: %d", primary_thread_cpu, cpuid);
                    //pthread_mutex_lock(&mutex_num_fa);
                    //--num_free_agents;
                    //__kmp_set_thread_roles2(global_tid, OMP_ROLE_NONE);
                    //pthread_mutex_unlock(&mutex_num_fa);
                }
            }

            /* Lend CPU if no more tasks */
            else if (DLB_ATOMIC_LD(&pending_tasks) == 0 && 
                     ((cpu_data[cpuid].ownership ==  BORROWED) || omptool_opts & OMPTOOL_OPTS_LEND)) {
                cb_disable_cpu(cpuid, NULL);
                warning("Primary thread %d, disabling and lending cpu: %d", primary_thread_cpu, cpuid);

                /* TODO: only lend free agents not part of the process mask */
                /*       or, depending on the ompt dlb policy */
                if (!CPU_ISSET(cpuid, &process_mask)) {
                    DLB_LendCpu(cpuid);
                }
                //pthread_mutex_lock(&mutex_num_fa);
                //--num_free_agents;
                //__kmp_set_thread_roles2(global_tid,OMP_ROLE_NONE);
                //pthread_mutex_unlock(&mutex_num_fa);
            }
        }
        instrument_event(BINDINGS_EVENT, 0, EVENT_END);
    }
}

