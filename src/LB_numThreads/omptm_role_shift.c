/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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
#include "LB_numThreads/omptool.h"

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
static int system_size;
static int default_num_threads;
static atomic_int num_free_agents = 0;

/* Masks */
static cpu_set_t active_mask;
static cpu_set_t process_mask;
static cpu_set_t primary_thread_mask;

/* Atomic variables */
static atomic_bool DLB_ALIGN_CACHE in_parallel = false;
static atomic_int  DLB_ALIGN_CACHE current_parallel_size = 0;
static atomic_uint DLB_ALIGN_CACHE pending_tasks = 0;

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
    _Atomic(cpu_status_t) ownership;
    bool         fa; 
} cpu_data_t;

static atomic_int registered_threads = 0; 
static cpu_data_t *cpu_data = NULL;
static int *cpu_by_id = NULL;

static int get_id_from_cpu(int cpuid){
    int i;
    int nth = DLB_ATOMIC_LD_RLX(&registered_threads);
    for(i = 0; i < nth; i++){
        if(cpu_by_id[i] == cpuid)
            return i;
    }
    return -1;
}

/*********************************************************************************/
/*  DLB callbacks                                                                */
/*********************************************************************************/

static void cb_enable_cpu(int cpuid, void *arg) {
    cas_bit((atomic_int *)&cpu_data[cpuid].ownership, LENT, OWN);
    cas_bit((atomic_int *)&cpu_data[cpuid].ownership, UNKNOWN, BORROWED);
    int pos = get_id_from_cpu(cpuid);
    if(pos >= 0){ //A thread was running here previously
        if(cpu_data[cpuid].fa){//We had a FA here. Call the api to wake it up and mark the CPU as free here
            DLB_ATOMIC_ADD(&num_free_agents, 1);
            __kmp_set_thread_roles2(pos, OMP_ROLE_FREE_AGENT);
        }
    }
    else if(pos == -1){//ask for a new FA
        cpu_data[cpuid].fa = false;
        DLB_ATOMIC_ADD(&num_free_agents, 1);
        __kmp_set_thread_roles2(system_size, OMP_ROLE_FREE_AGENT);
        cpu_by_id[DLB_ATOMIC_ADD(&registered_threads, 1)] = cpuid;
    }
    else{
        fatal("Enable cpu with a wrong pos");
    }
}

static void cb_disable_cpu(int cpuid, void *arg) {
    if((DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) != OWN &&
                 DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) != BORROWED)){
        //CPU already disabled, just skip it
        return;
    }
    cas_bit((atomic_int *)&cpu_data[cpuid].ownership, OWN, LENT);
    cas_bit((atomic_int *)&cpu_data[cpuid].ownership, BORROWED, UNKNOWN);
    if(cpu_data[cpuid].fa){
        int tid = get_id_from_cpu(cpuid);
        if(tid >= 0){
            DLB_ATOMIC_SUB(&num_free_agents, 1);
            __kmp_set_thread_roles2(tid, OMP_ROLE_NONE);
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
                CPU_SET(i, &primary_thread_mask);
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
}

/*********************************************************************************/
/*  Blocking calls specific functions                                           */
/*********************************************************************************/

/*TODO: what happens when executing in "ompss" mode*/
void omptm_role_shift__IntoBlockingCall(void) {
    if (lewi) {
        /* Don't know what to do if a Blocking Call is invoked inside a
         * parallel region. We could ignore it, but then we should also ignore
         * the associated OutOfBlockingCall, and how would we know it? */
        fatal_cond(DLB_ATOMIC_LD(&in_parallel),
                "Blocking Call inside a parallel region not supported");
        cpu_set_t cpus_to_lend;
        CPU_ZERO(&cpus_to_lend);
        int i;
        for(i = 0; i < system_size; i++){
            if(DLB_ATOMIC_LD_RLX(&cpu_data[i].ownership) == OWN){
                if(i == cpu_by_id[global_tid]){
                    //Just change the status for the thread calling MPI
                    DLB_ATOMIC_ST_RLX(&cpu_data[i].ownership, LENT);
                }
                else{
                    cb_disable_cpu(i, NULL);
                }
                CPU_SET(i, &cpus_to_lend);
            }
            else if(DLB_ATOMIC_LD_RLX(&cpu_data[i].ownership) == BORROWED && CPU_ISSET(i, &process_mask)){
                DLB_ATOMIC_ST_RLX(&cpu_data[i].ownership, UNKNOWN);
                CPU_SET(i, &cpus_to_lend);
            }
        }
        DLB_LendCpuMask(&cpus_to_lend);
        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}


void omptm_role_shift__OutOfBlockingCall(void) {
    if (lewi) {
        cb_enable_cpu(cpu_by_id[global_tid], NULL);
        if (omptool_opts & OMPTOOL_OPTS_LEND) {
            /* Do nothing.
             * Do not reclaim since going out of a blocking call is not
             * an indication that the CPUs may be needed. 
             * OMPTOOL_OPTS_AGGRESSIVE executes this if too. */
        }
        else {
            DLB_Reclaim();
        }
    }
}


/*********************************************************************************/
/*  OMPT registered callbacks                                                    */
/*********************************************************************************/

void omptm_role_shift__thread_begin(ompt_thread_t thread_type) {
    /* Set up thread local spd */
    spd_enter_dlb(thread_spd);

    global_tid = __kmp_get_thread_id();
    fatal_cond(registered_threads > system_size,
            "DLB created more threads than existing CPUs in the node");

    int cpuid = cpu_by_id[global_tid];
    if(thread_type == ompt_thread_other){ //other => free agent
        cpu_set_t thread_mask;
        CPU_ZERO(&thread_mask);
        if(cpuid >= 0 && (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) != OWN &&
                          DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) != LENT)){
            //Bind the thread to the pre-assigned CPU and return the CPU after that if necessary
            cpu_data[cpuid].fa = true;
            cpu_by_id[global_tid] = cpuid;
            CPU_SET(cpuid, &thread_mask);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            verbose(VB_OMPT, "Binding a free agent to CPU %d", cpuid);
            instrument_event(REBIND_EVENT, cpuid+1, EVENT_BEGIN);
            if (lewi) {
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
        else{
            pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            cpuid = mu_get_single_cpu(&thread_mask);
            if(cpuid != -1){
                cpu_by_id[global_tid] = cpuid;
                cpu_data[cpuid].fa = true;
            }
            else
                warning("Started a free agent with multiple CPUs in the affinity mask");
        }
    }
}

void omptm_role_shift__thread_role_shift(ompt_data_t *thread_data,
                                         ompt_role_t prior_role,
                                         ompt_role_t next_role){
    int cpuid = cpu_by_id[global_tid];
    if(prior_role == OMP_ROLE_FREE_AGENT){
        if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
        verbose(VB_OMPT, "Free agent %d changing the role to NONE", global_tid);
    }
    else if(prior_role == OMP_ROLE_NONE){
        if(next_role == OMP_ROLE_COMMUNICATOR) return; //Don't supported now
        if(CPU_ISSET(cpuid, &process_mask)) //One of the initial/worker threads. Don't need to check for own CPUs.
            return;
        if (lewi) {
            if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
                if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                    cb_disable_cpu(cpuid, NULL);
                }
            }
            else if (DLB_ATOMIC_LD(&pending_tasks) == 0
                    && (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) == BORROWED
                        || omptool_opts & OMPTOOL_OPTS_LEND)) {
                cb_disable_cpu(cpuid, NULL);
                DLB_LendCpu(cpuid);
            }
        }
    }
}


void omptm_role_shift__parallel_begin(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        DLB_ATOMIC_ST(&in_parallel, true);
        DLB_ATOMIC_ST(&current_parallel_size, parallel_data->requested_parallelism);
    }
}

void omptm_role_shift__parallel_end(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        DLB_ATOMIC_ST(&in_parallel, false);
    }
}

void omptm_role_shift__task_create(void) {
    /* Increment the amount of pending tasks */
    DLB_ATOMIC_ADD(&pending_tasks, 1);

    /* For now, let's assume that we always want to increase the number
        * of active threads whenever a task is created
        */
    if (lewi) {
        if(omptool_opts == OMPTOOL_OPTS_LEND) {
            DLB_BorrowCpus(1);
        }
        else {
            DLB_AcquireCpus(1);
        }
    }
}

void omptm_role_shift__task_complete(void) {
    int cpuid = cpu_by_id[global_tid];
    if (lewi && cpuid >= 0 && cpu_data[cpuid].fa) {
        /* Return CPU if reclaimed */
        if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
            if(DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) == UNKNOWN) {
                /* Previously we have returned the CPU, but the free agent
                 * didn't do a role shift event to be rescheduled This can
                 * happen when the thread receives a change from NONE to FA
                 * just after a FA to NONE change. In that case, the second
                 * shift cancels the first one and the thread doesn't emit a
                 * callback. Just deactivate the thread. */
                DLB_ATOMIC_SUB(&num_free_agents, 1);
                __kmp_set_thread_roles2(global_tid, OMP_ROLE_NONE);
            }
            else if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                cb_disable_cpu(cpuid, NULL);
            }
        }
        /* Lend CPU if no more tasks and CPU is borrowed, or policy is LEND */
        else if (DLB_ATOMIC_LD(&pending_tasks) == 0
                && (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].ownership) == BORROWED
                    || omptool_opts & OMPTOOL_OPTS_LEND)) {
            cb_disable_cpu(cpuid, NULL);
            DLB_LendCpu(cpuid);
        }
    }
}

void omptm_role_shift__task_switch(void) {
    if (lewi && DLB_ATOMIC_SUB(&pending_tasks, 1) > 1) {
        if(omptool_opts == OMPTOOL_OPTS_LEND) {
            DLB_BorrowCpus(1);
        } else {
            DLB_AcquireCpus(1);
        }
    }
}


/*********************************************************************************/
/*    Functions for testing purposes                                             */
/*********************************************************************************/

int omptm_role_shift_testing__get_num_free_agents(void) {
    return num_free_agents;
}

int omptm_role_shift_testing__get_num_registered_threads(void) {
    return registered_threads;
}

int omptm_role_shift_testing__get_current_parallel_size(void) {
    return current_parallel_size;
}

void omptm_role_shift_testing__set_pending_tasks(unsigned int num_tasks) {
    pending_tasks = num_tasks;
}

unsigned int omptm_role_shift_testing__get_pending_tasks(void) {
    return pending_tasks;
}

void omptm_role_shift_testing__set_global_tid(int tid) {
    global_tid = tid;
}

bool omptm_role_shift_testing__in_parallel(void) {
    return in_parallel;
}

int omptm_role_shift_testing__get_id_from_cpu(int cpuid) {
    return get_id_from_cpu(cpuid);
}

int* omptm_role_shift_testing__get_cpu_by_id_ptr(void) {
    return cpu_by_id;
}

cpu_data_t* omptm_role_shift_testing__get_cpu_data_ptr(void) {
    return cpu_data;
}
