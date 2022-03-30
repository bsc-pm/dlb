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

typedef struct DLB_ALIGN_CACHE CPU_Data {
    cpu_status_t ownership;
    bool         fa; 
} cpu_data_t;

static atomic_int registered_threads = 0; 
static cpu_data_t *cpu_data = NULL;
static int *cpu_by_id = NULL;

static unsigned int get_thread_id(void){
    unsigned int a = (unsigned int)__kmp_get_thread_id();
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
    if(cpu_data[cpuid].ownership == LENT){ 
        //We are reclaiming a previousley LENT cpu
        cpu_data[cpuid].ownership = OWN;
        return;
    }
    else if(cpu_data[cpuid].ownership == UNKNOWN){
        cpu_data[cpuid].ownership = BORROWED;
    }
    int pos = get_id_from_cpu(cpuid);
    if(pos >= 0){ //A thread was running here previously
        if(cpu_data[cpuid].fa){//We had a FA here. Call the api to wake it up and mark the CPU as free here
            pthread_mutex_lock(&mutex_num_fa);
            ++num_free_agents;
            __kmp_set_thread_roles2(pos, OMP_ROLE_FREE_AGENT);
            pthread_mutex_unlock(&mutex_num_fa);
        }
    }
    else if(pos == -1){//ask for a new FA
        cpu_data[cpuid].fa = false;
        pthread_mutex_lock(&mutex_num_fa);
        if(num_free_agents == registered_threads){
            if(DLB_ATOMIC_LD_RLX(&in_parallel)){
                if(num_free_agents < DLB_ATOMIC_LD_RLX(&current_parallel_size)){
                    num_free_agents = DLB_ATOMIC_LD_RLX(&current_parallel_size);
                }
            }
            else{
                num_free_agents = max_int(__kmp_get_num_threads_role(OMP_ROLE_FREE_AGENT), 1);
            }
            ++num_free_agents;
            __kmp_set_thread_roles1(num_free_agents, OMP_ROLE_FREE_AGENT);
            cpu_by_id[registered_threads] = cpuid;
            DLB_ATOMIC_ADD(&registered_threads, 1);            
        }
        else{
            __kmp_set_thread_roles2(system_size, OMP_ROLE_FREE_AGENT);
            cpu_by_id[registered_threads] = cpuid;
            DLB_ATOMIC_ADD(&registered_threads, 1);
        }
        pthread_mutex_unlock(&mutex_num_fa);
    }
    else{
        fatal("Enable cpu with a wrong pos");
    }
}

static void cb_disable_cpu(int cpuid, void *arg) {
    fatal_cond(!(cpu_data[cpuid].ownership == OWN || cpu_data[cpuid].ownership == BORROWED),
            "Disabling an already disabled CPU");
	if(cpu_data[cpuid].ownership == BORROWED){
	    cpu_data[cpuid].ownership = UNKNOWN;
	}
	else if(cpu_data[cpuid].ownership == OWN){
	    cpu_data[cpuid].ownership = LENT;
	}
	if(cpu_data[cpuid].fa){
        int tid = get_id_from_cpu(cpuid);
        if(tid >= 0){
            pthread_mutex_lock(&mutex_num_fa);
            --num_free_agents;
            __kmp_set_thread_roles2(tid, OMP_ROLE_NONE);
            pthread_mutex_unlock(&mutex_num_fa);
        }
	}
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
}

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
    registered_threads = (default_num_threads > num_free_agents) ? default_num_threads : num_free_agents;
    
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
				primary_thread_cpu = i;
				CPU_SET(i, &primary_thread_mask);
				cpu_by_id[encountered_cpus - 1] = i;
			}
			else if(encountered_cpus <= num_workers){
				//Assume the next CPUs after the primary thread are for the workers and are not free.
				cpu_by_id[encountered_cpus - 1] = i;
			}
			cpu_data[i].ownership = OWN;
		}
		else{
			cpu_data[i].ownership = UNKNOWN;
		}
		cpu_data[i].fa = false;
	}
	memcpy(&active_mask, &primary_thread_mask, sizeof(cpu_set_t));

	//Extrae functions configuration
	if(Extrae_change_num_threads){
	    Extrae_change_num_threads(system_size);
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
    }
}

void omptm_role_shift__finalize(void) {
	free(cpu_data);
	cpu_data = NULL;
	free(cpu_by_id);
	cpu_by_id = NULL;
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
        fatal_cond(DLB_ATOMIC_LD(&in_parallel),
                "Blocking Call inside a parallel region not supported");
        cpu_set_t cpus_to_lend;
        CPU_ZERO(&cpus_to_lend);
        int i;
        for(i = 0; i < system_size; i++){
            if(cpu_data[i].ownership == OWN){
                cpu_data[i].ownership = LENT;
                CPU_SET(i, &cpus_to_lend);
            }
            else if(cpu_data[i].ownership == BORROWED && CPU_ISSET(i, &process_mask)){
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
            cb_enable_cpu(cpu_by_id[global_tid], NULL);
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
	global_tid = __kmp_get_thread_id();
	
    fatal_cond(registered_threads > system_size,
            "DLB created more threads than existing CPUs in the node");
	
	if(thread_type == ompt_thread_other){ //other => free agent
        cpu_set_t thread_mask;
        int cpuid = cpu_by_id[global_tid];
        if(cpuid == -1) //One of the initial threads initialize as a free agent. Don't bind them.
            return;
        //Bind the thread to the pre-assigned CPU and return the CPU after that if necessary
        cpu_data[cpuid].fa = true;
        cpu_by_id[global_tid] = cpuid;
        CPU_ZERO(&thread_mask);
        CPU_SET(cpuid, &thread_mask);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
        verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
        instrument_event(REBIND_EVENT, cpuid+1, EVENT_BEGIN);
        if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
            if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                cb_disable_cpu(cpuid, NULL);
            }
        }
        else if (DLB_ATOMIC_LD(&pending_tasks) == 0) {
            cb_disable_cpu(cpuid, NULL);
            /* TODO: only lend free agents not part of the process mask */
            /*       or, depending on the ompt dlb policy */
            if (!CPU_ISSET(cpuid, &process_mask)) {
                DLB_LendCpu(cpuid);
            }
        }
	}
}

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
        int cpuid = cpu_by_id[global_tid];
        if(cpuid == -1) //One of the initial threads. Don't need to check for own CPUs.
            return;
        if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
            if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                cb_disable_cpu(cpuid, NULL);
            }
        }
        else if (DLB_ATOMIC_LD(&pending_tasks) == 0) {
            cb_disable_cpu(cpuid, NULL);
            /* TODO: only lend free agents not part of the process mask */
            /*       or, depending on the ompt dlb policy */
            if (!CPU_ISSET(cpuid, &process_mask)) {
                DLB_LendCpu(cpuid);
            }
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
    }
}

void omptm_role_shift__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (parallel_data->value == PARALLEL_LEVEL_1) {
        parallel_data->value = PARALLEL_UNSET;
        DLB_ATOMIC_ST(&in_parallel, false);
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
    }
}

void omptm_role_shift__task_schedule(
        ompt_data_t *prior_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *next_task_data) {
    if (prior_task_status == ompt_task_switch) {
        if (DLB_ATOMIC_SUB(&pending_tasks, 1) > 1) {
            DLB_AcquireCpus(1);
        }
        instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
    } else if (prior_task_status == ompt_task_complete) {
        int cpuid = cpu_by_id[global_tid];
        if (cpuid >= 0 && cpu_data[cpuid].fa) {
            /* Return CPU if reclaimed */
            if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
                if(cpu_data[cpuid].ownership == UNKNOWN){
                    /* Previously we have returned the CPU, but the free agent didn't do a role shift event to be rescheduled
                     * This can happen when the thread receives a change from NONE to FA just after a FA to NONE change. In that case,
                     * the second shift cancels the first one and the thread doesn't emit a callback. Just deactivate the thread. */
                    pthread_mutex_lock(&mutex_num_fa);
                    --num_free_agents;
                    printf("Primary thread %d, decreasing the number of free agents to %d in task complete. Deactivating thread %d\n", primary_thread_cpu, num_free_agents, global_tid);
                    __kmp_set_thread_roles2(global_tid, OMP_ROLE_NONE);
                    pthread_mutex_unlock(&mutex_num_fa);
                }
                else if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
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

