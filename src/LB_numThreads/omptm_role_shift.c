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

/* Masks */
static cpu_set_t active_mask;
static cpu_set_t process_mask;
static cpu_set_t primary_thread_mask;
//static cpu_set_t worker_threads_mask;

/* Atomic variables */
static atomic_bool DLB_ALIGN_CACHE in_parallel = false;
static atomic_uint DLB_ALIGN_CACHE pending_tasks = 0;
static atomic_int  DLB_ALIGN_CACHE num_free_agents = 0;
static atomic_int  DLB_ALIGN_CACHE parallel_team_size = 0;

/* Thread local */
__thread int __binding = -1;

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
    atomic_bool free_cpu; //True: Unused, False: in use
    bool        fa; //Indicates if a free agent is running in this cpu
} cpu_data_t;

static cpu_data_t *cpu_data = NULL;


/*********************************************************************************/
/*  DLB callbacks                                                                */
/*********************************************************************************/

static void cb_enable_cpu(int cpuid, void *arg) {
    if(cpu_data[cpuid].ownership == LENT){ 
        //We are reclaiming a previousley LENT cpu
        cpu_data[cpuid].ownership = OWN;
    }
    else if(cpu_data[cpuid].ownership == UNKNOWN){
        cpu_data[cpuid].ownership = BORROWED;
        cpu_data[cpuid].free_cpu = true;
    }
    cpu_data[cpuid].fa = false;
}

static void cb_disable_cpu(int cpuid, void *arg) {
  //TODO: Lock here??
	if(cpu_data[cpuid].fa){
		DLB_ATOMIC_SUB(&num_free_agents, 1);
		__kmp_set_thread_roles1(num_free_agents, OMP_ROLE_FREE_AGENT);
	}
	if(cpu_data[cpuid].ownership == BORROWED)
	    cpu_data[cpuid].ownership = UNKNOWN;
	//cpu_data[cpuid].free_cpu = false;
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
}

/*********************************************************************************/
/*  Other static functions                                                       */
/*********************************************************************************/

/* Actions to do when --lewi-ompt=lend */
//static void omptm_free_agents__lend(void) {
//}

/* Look for local available CPUs, if none is found ask LeWI */
static void acquire_one_free_agent(void) {
	if(lewi){
		if(DLB_AcquireCpus(1) == DLB_SUCCESS){
			//TODO: Need a lock here??
			int fa = DLB_ATOMIC_LD_RLX(&num_free_agents);
			__kmp_set_thread_roles1(++fa, OMP_ROLE_FREE_AGENT);
			DLB_ATOMIC_ST(&num_free_agents, fa);
		}
	}
}

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
    
    CPU_ZERO(&primary_thread_mask);
    int num_workers = default_num_threads - num_free_agents;
    int encountered_cpus = 0;
    int i;
    for(i = 0; i < system_size; i++){
    	if(CPU_ISSET(i, &process_mask)){
    		if(++encountered_cpus == 1){
    			//First encountered CPU belongs to the primary thread
					cpu_data[i].free_cpu = false;
					primary_thread_cpu = i;
					CPU_SET(i, &primary_thread_mask);
			}
			else if(encountered_cpus < num_workers){
				//Assume the next CPUs after the primary thread are for the workers and are not free.
				cpu_data[i].free_cpu = false;
			}
			else{
				//Don't assume a CPU will be used until starting some thread in it.
			    cpu_data[i].free_cpu = true;
			}
			cpu_data[i].ownership = OWN;
		}
		else{
			cpu_data[i].ownership = UNKNOWN;
			cpu_data[i].free_cpu = false;
		}
		cpu_data[i].fa = false;
	}
	memcpy(&active_mask, &primary_thread_mask, sizeof(cpu_set_t));
    
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
            DLB_Reclaim();
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
        CPU_ZERO(&thread_mask);
        CPU_SET(cpuid, &thread_mask);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
        __binding = cpuid;
        verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
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
	//int cpuid = thread_data->value;
	if(prior_role == OMP_ROLE_FREE_AGENT){
		if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
		fatal_cond(__binding < 0, "free agent cpuid < 0");
		//fatal_cond(!cpu_data[__binding].fa, "unregistered free agent shifting to worker");
		
		cpu_data[__binding].fa = false;
		//thread_data->value = -1;
		
		//TODO: Do the runtime call the thread role shift callback before the task schedule or viceversa??
		if(cpu_data[__binding].ownership != OWN ||
		   cpu_data[__binding].ownership != BORROWED){//We don't longer have this CPU
		}
	}
	else if(prior_role == OMP_ROLE_NONE){
		if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
		cpu_set_t thread_mask;
		int cpuid;
		for(cpuid = 0; cpuid < system_size; cpuid++){
			if((cpu_data[cpuid].ownership == OWN || cpu_data[cpuid].ownership == BORROWED) &&
			    cpu_data[cpuid].free_cpu){
				
				cpu_data[cpuid].free_cpu = false;
				cpu_data[cpuid].fa = true;
				//cpuid = i;
				break;
			}
		}
		CPU_ZERO(&thread_mask);
		CPU_SET(cpuid, &thread_mask);
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
		thread_data->value = cpuid;
		__binding = cpuid;
		instrument_event(BINDINGS_EVENT, cpuid+1, EVENT_BEGIN);
		verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
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
        DLB_ATOMIC_ST(&parallel_team_size, requested_parallelism);
        DLB_ATOMIC_ADD(&num_free_agents, requested_parallelism);
    }
}

void omptm_role_shift__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (parallel_data->value == PARALLEL_LEVEL_1) {
        parallel_data->value = PARALLEL_UNSET;
        DLB_ATOMIC_SUB(&num_free_agents, DLB_ATOMIC_LD(&parallel_team_size));
        //TODO:Maybe the next store isn't needed
        DLB_ATOMIC_ST(&parallel_team_size, 0);
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
        acquire_one_free_agent();
    }
}

void omptm_role_shift__task_schedule(
        ompt_data_t *prior_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *next_task_data) {
		int cpuid = __binding;
    if (prior_task_status == ompt_task_switch) {
        if(cpu_data[cpuid].fa && (cpu_data[cpuid].ownership != OWN ||
                                  cpu_data[cpuid].ownership != BORROWED)){
			/* We have disabled the running CPU of the FA but the runtime didn't
			 * turn off the thread, must rebind it */
			cpu_data[cpuid].fa = false;
			int i;
			for(i = 0; i < system_size; i++){
				if((cpu_data[i].ownership == OWN || cpu_data[i].ownership == BORROWED) &&
				    cpu_data[i].free_cpu){
					
					cpu_data[i].free_cpu = false;
					cpu_data[i].fa = true;
					cpuid = i;
					break;
				}
			}
			cpu_set_t thread_mask;
			CPU_ZERO(&thread_mask);
			CPU_SET(cpuid, &thread_mask);
			pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
			__binding = cpuid;
			//TODO: some instrument event here
		}
        if (DLB_ATOMIC_SUB(&pending_tasks, 1) > 1) {
            acquire_one_free_agent();
        }
        instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
    } else if (prior_task_status == ompt_task_complete) {
        if (cpu_data[cpuid].fa) {
            /* Return CPU if reclaimed */
            if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
                if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                    cb_disable_cpu(cpuid, NULL);
                }
            }

            /* Lend CPU if no more tasks */
            else if (DLB_ATOMIC_LD(&pending_tasks) == 0 && 
                     ((cpu_data[cpuid].ownership ==  BORROWED) || omptool_opts & OMPTOOL_OPTS_LEND)) {
                cb_disable_cpu(cpuid, NULL);

                /* TODO: only lend free agents not part of the process mask */
                /*       or, depending on the ompt dlb policy */
                if (!CPU_ISSET(cpuid, &process_mask)) {
                    DLB_LendCpu(cpuid);
                }
            }
        }
        instrument_event(BINDINGS_EVENT, 0, EVENT_END);
    }
}

