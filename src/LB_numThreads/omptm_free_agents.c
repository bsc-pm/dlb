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

#include "LB_numThreads/omptm_free_agents.h"

#include "apis/dlb.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
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
int __kmp_get_free_agent_id(void) __attribute__((weak));
int __kmp_get_num_free_agent_threads(void) __attribute__((weak));
void __kmp_set_free_agent_thread_active_status(
        unsigned int thread_num, bool active) __attribute__((weak));

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

/* Masks */
static cpu_set_t active_mask;
static cpu_set_t process_mask;
static cpu_set_t primary_thread_mask;
static cpu_set_t worker_threads_mask;

/* Atomic variables */
static atomic_bool DLB_ALIGN_CACHE in_parallel = false;
static atomic_uint DLB_ALIGN_CACHE pending_tasks = 0;
static atomic_int  DLB_ALIGN_CACHE num_enabled_free_agents = 0;

/* Thread local */
__thread int __free_agent_id = -1;
__thread int __worker_binding = -1;

/*********************************************************************************/
/*  Free agent CPU lists for fast indexing                                       */
/*********************************************************************************/

/* Free agent lists */
static int num_free_agents;
static int *free_agent_id_by_cpuid = NULL;      /* indexed by CPU id */
static int *free_agent_cpuid_by_id = NULL;      /* indexed by free agent id */
static int *free_agent_cpu_list = NULL;         /* CPUs where a free agent is bound, owned first */

/* Lock for all the above lists, they should only be written on thread creation */
static pthread_rwlock_t free_agent_list_lock;

static void free_agent_lists_init(void) {
    pthread_rwlock_init(&free_agent_list_lock, NULL);
    pthread_rwlock_wrlock(&free_agent_list_lock);
    free_agent_id_by_cpuid = malloc(sizeof(int)*system_size);
    free_agent_cpuid_by_id = malloc(sizeof(int)*num_free_agents);
    free_agent_cpu_list = malloc(sizeof(int)*num_free_agents);
    int i;
    for (i=0; i<system_size; ++i) free_agent_id_by_cpuid[i] = -1;
    for (i=0; i<num_free_agents; ++i) free_agent_cpuid_by_id[i] = -1;
    for (i=0; i<num_free_agents; ++i) free_agent_cpu_list[i] = -1;
    pthread_rwlock_unlock(&free_agent_list_lock);
}

static void free_agent_lists_destroy(void) {
    pthread_rwlock_wrlock(&free_agent_list_lock);
    free(free_agent_id_by_cpuid);
    free_agent_id_by_cpuid = NULL;
    free(free_agent_cpuid_by_id);
    free_agent_cpuid_by_id = NULL;
    free(free_agent_cpu_list);
    free_agent_cpu_list = NULL;
    pthread_rwlock_unlock(&free_agent_list_lock);
    pthread_rwlock_destroy(&free_agent_list_lock);
}

static void free_agent_lists_register(int thread_id, int cpuid) {
    pthread_rwlock_wrlock(&free_agent_list_lock);
    /* Insert id's */
    free_agent_id_by_cpuid[cpuid] = thread_id;
    free_agent_cpuid_by_id[thread_id] = cpuid;

    /* Insert CPUid and reorder */
    int i;
    for (i=0; i<num_free_agents; ++i) {
        if (free_agent_cpu_list[i] == -1) {
            free_agent_cpu_list[i] = cpuid;
            break;
        }
    }
    qsort_r(free_agent_cpu_list, i+1, sizeof(int),
            mu_cmp_cpuids_by_ownership, &process_mask);

    pthread_rwlock_unlock(&free_agent_list_lock);
}

static inline int get_free_agent_id_by_cpuid(int cpuid) {
    int id = -1;
    pthread_rwlock_rdlock(&free_agent_list_lock);
    if (likely(free_agent_id_by_cpuid != NULL)) {
        id = free_agent_id_by_cpuid[cpuid];
    }
    pthread_rwlock_unlock(&free_agent_list_lock);
    return id;
}

static inline int get_free_agent_cpuid_by_id(int thread_id) {
    int cpuid = -1;
    pthread_rwlock_rdlock(&free_agent_list_lock);
    if (likely(free_agent_cpuid_by_id != NULL)) {
        cpuid = free_agent_cpuid_by_id[thread_id];
    }
    pthread_rwlock_unlock(&free_agent_list_lock);
    return cpuid;
}

/*********************************************************************************/
/*  CPU Data structures and helper atomic flags functions                        */
/*********************************************************************************/

/* Current state of the CPU (what is being used for) */
typedef enum CPUState {
    CPU_STATE_UNKNOWN               = 0,
    CPU_STATE_IDLE                  = 1 << 0,
    CPU_STATE_LENT                  = 1 << 1,
    CPU_STATE_RECLAIMED             = 1 << 2,
    CPU_STATE_IN_PARALLEL           = 1 << 3,
    CPU_STATE_FREE_AGENT_ENABLED    = 1 << 4,
} cpu_state_t;

/* Possible OpenMP roles that the CPU can take */
typedef enum OpenMP_Roles {
    ROLE_NONE       = 0,
    ROLE_PRIMARY    = 1 << 0,
    ROLE_WORKER     = 1 << 1,
    ROLE_FREE_AGENT = 1 << 2,
} openmp_roles_t;

typedef struct DLB_ALIGN_CACHE CPU_Data {
    openmp_roles_t       roles;
    _Atomic(cpu_state_t) state;
    atomic_bool          wanted_for_parallel;
} cpu_data_t;

static cpu_data_t *cpu_data = NULL;


/*********************************************************************************/
/*  DLB callbacks                                                                */
/*********************************************************************************/

static void cb_enable_cpu(int cpuid, void *arg) {
    /* Skip callback if this CPU is required for a parallel region */
    if (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].wanted_for_parallel)) {
        return;
    }

    /* If this CPU is reclaimed, set IDLE */
    if (cas_bit((atomic_int*)&cpu_data[cpuid].state,
                CPU_STATE_RECLAIMED, CPU_STATE_IDLE)) {
        return;
    }

    int free_agent_id = get_free_agent_id_by_cpuid(cpuid);
    if (unlikely(free_agent_id == -1)) {
        /* probably too early? */
        DLB_LendCpu(cpuid);
    } else {
        /* Enable associated free agent thread if not already */
        if(test_set_clear_bit((atomic_int*)&cpu_data[cpuid].state,
                    CPU_STATE_FREE_AGENT_ENABLED, CPU_STATE_IDLE)) {
            DLB_ATOMIC_ADD(&num_enabled_free_agents, 1);
            verbose(VB_OMPT, "Enabling free agent %d", free_agent_id);
            __kmp_set_free_agent_thread_active_status(free_agent_id, true);
        }
    }
}

static void cb_disable_cpu(int cpuid, void *arg) {
    int free_agent_id = get_free_agent_id_by_cpuid(cpuid);
    if (unlikely(free_agent_id == -1)) {
        /* Probably a callback after ompt_finalize has been called, ignore */
        return;
    }

    /* If CPU is not needed, set IDLE */
    if (CPU_ISSET(cpuid, &process_mask)
            && !DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].wanted_for_parallel)) {
        set_bit((atomic_int*)&cpu_data[cpuid].state, CPU_STATE_IDLE);
    }

    /* If CPU was assigned to a free agent thread, disable it */
    if (clear_bit((atomic_int*)&cpu_data[cpuid].state, CPU_STATE_FREE_AGENT_ENABLED)) {
        DLB_ATOMIC_SUB(&num_enabled_free_agents, 1);
        verbose(VB_OMPT, "Disabling free agent %d", free_agent_id);
        __kmp_set_free_agent_thread_active_status(free_agent_id, false);
    }
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    memcpy(&process_mask, mask, sizeof(cpu_set_t));
    memcpy(&active_mask, mask, sizeof(cpu_set_t));
}

/*********************************************************************************/
/*  Other static functions                                                       */
/*********************************************************************************/

/* Actions to do when --lewi-ompt=lend */
static void omptm_free_agents__lend(void) {
    if (lewi && omptool_opts & OMPTOOL_OPTS_LEND) {
        /* Lend all IDLE worker CPUs */
        cpu_set_t mask;
        int cpuid;
        for (cpuid=0; cpuid<system_size; ++cpuid) {
            if (CPU_ISSET(cpuid, &worker_threads_mask) &&
                    cas_bit((atomic_int*)&cpu_data[cpuid].state,
                        CPU_STATE_IDLE, CPU_STATE_LENT)) {
                CPU_SET(cpuid, &mask);
            }
        }
        DLB_LendCpuMask(&mask);

        /* The active mask should only be the primary mask */
        memcpy(&active_mask, &primary_thread_mask, sizeof(cpu_set_t));

        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
    }
}

/* Look for local available CPUs, if none is found ask LeWI */
static void acquire_one_free_agent(void) {
    if (num_enabled_free_agents == num_free_agents) {
        return;
    }

    cpu_set_t cpus_to_ask;
    CPU_ZERO(&cpus_to_ask);

    /* Iterate only CPUs where free agents are assigned */
    int i;
    pthread_rwlock_rdlock(&free_agent_list_lock);
    for (i=0; i<num_free_agents; ++i) {
        int cpuid = free_agent_cpu_list[i];

        /* It is safe to just make a copy, we either skip the CPU or
         * call enable_cpu which will do an atomic exchange */
        cpu_state_t cpu_state = DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].state);

        /* Skip CPU if it's already busy */
        if (cpu_state & (CPU_STATE_IN_PARALLEL | CPU_STATE_FREE_AGENT_ENABLED)) {
            // FIXME: check other flags? wanted_for_parallel?
            continue;
        }

        /* If some CPU is IDLE, try enabling it */
        if (cpu_state == CPU_STATE_IDLE) {
            cb_enable_cpu(cpuid, NULL);
            CPU_ZERO(&cpus_to_ask);
            break;
        } else {
            CPU_SET(cpuid, &cpus_to_ask);
        }
    }
    pthread_rwlock_unlock(&free_agent_list_lock);

    /* Call LeWI if we didn't find any IDLE CPU */
    if (lewi && CPU_COUNT(&cpus_to_ask) > 0) {
        DLB_AcquireCpusInMask(1, &cpus_to_ask);
    }
}

/* Obtain a CPU id for a given free agent id */
static int get_free_agent_binding(int thread_id) {
    /* Find out a CPU for the free agent thread */
    int cpuid = -1;

    /* FIXME: If the process mask, and number of workers and free agent threads
     *          are immutable, this function could be called only once, and
     *          save all the possible bindings for each free agent thread.
     *          (but not during init, we may not have all CPUs registered yet)
     */

    /* FIXME: let's assume that all processes are already registered when the
     *          first free agent starts. Otherwise, we wouldn't be able to
     *          take the right decision here.
     */

    cpu_set_t available_process_cpus;
    mu_substract(&available_process_cpus, &process_mask, &primary_thread_mask);
    mu_substract(&available_process_cpus, &available_process_cpus, &worker_threads_mask);
    int num_free_agents_in_available_cpus = mu_count(&available_process_cpus);
    if (thread_id < num_free_agents_in_available_cpus) {
        /* Simpler scenario: the default workers mask plus primary does not cover
         * all the CPUs in the process mask and the free agent thread_id is low
         * enough to use one of those free CPUs.
         */
        int cpus_found = 0;
        for (cpuid=0; cpuid<system_size; ++cpuid) {
            if (CPU_ISSET(cpuid, &available_process_cpus)) {
                if (cpus_found++ == thread_id) {
                    break;
                }
            }
        }
    }

    else {
        int num_non_owned_cpus = shmem_cpuinfo__get_number_of_non_owned_cpus(pid);
        if (thread_id < (num_non_owned_cpus + num_free_agents_in_available_cpus)) {
            /* Second case: Find a CPU in another process */
            cpuid = shmem_cpuinfo__get_nth_non_owned_cpu(pid,
                    thread_id - num_free_agents_in_available_cpus);
        } else if (thread_id < (num_non_owned_cpus + num_free_agents_in_available_cpus
                    + CPU_COUNT(&worker_threads_mask))) {
            /* Third case: Share a CPU with some worker thread */
            int victim_worker = thread_id
                - num_non_owned_cpus - num_free_agents_in_available_cpus;
            int workers_found = 0;
            for (cpuid=0; cpuid<system_size; ++cpuid) {
                if (CPU_ISSET(cpuid, &worker_threads_mask)) {
                    if (workers_found++ == victim_worker) {
                        break;
                    }
                }
            }
        } else {
            /* Last case: no CPUs left? */
        }
    }
    return cpuid;
}


/*********************************************************************************/
/*  Init & Finalize module                                                       */
/*********************************************************************************/

void omptm_free_agents__init(pid_t process_id, const options_t *options) {

    /* Initialize static variables */
    system_size = mu_get_system_size();
    lewi = options->lewi;
    omptool_opts = options->lewi_ompt;
    pid = process_id;
    num_free_agents = __kmp_get_num_free_agent_threads();
    shmem_procinfo__getprocessmask(pid, &process_mask, 0);
    verbose(VB_OMPT, "Process mask: %s", mu_to_str(&process_mask));

    // omp_get_max_threads cannot be called here, try using the env. var.
    const char *env_omp_num_threads = getenv("OMP_NUM_THREADS");
    int default_num_threads =
        env_omp_num_threads ? atoi(env_omp_num_threads)
                            : CPU_COUNT(&process_mask);

    /* Initialize atomic variables */
    DLB_ATOMIC_ST(&num_enabled_free_agents, 0);

    /* Initialize CPU Data array */
    cpu_data = malloc(sizeof(cpu_data_t)*system_size);

    /* Construct Primary and Worker threads masks */
    CPU_ZERO(&primary_thread_mask);
    CPU_ZERO(&worker_threads_mask);
    int cpuid;
    int encountered_cpus = 0;
    for (cpuid=0; cpuid<system_size; ++cpuid) {
        if (CPU_ISSET(cpuid, &process_mask)) {
            if (++encountered_cpus == 1) {
                /* First encountered CPU belongs to the primary thread */
                CPU_SET(cpuid, &primary_thread_mask);
                cpu_data[cpuid].roles = ROLE_PRIMARY;
                cpu_data[cpuid].state = CPU_STATE_IN_PARALLEL;
            } else if (encountered_cpus-1 < default_num_threads) {
                /* Infer the worker threads CPUS */
                CPU_SET(cpuid, &worker_threads_mask);
                cpu_data[cpuid].roles = ROLE_WORKER;
                cpu_data[cpuid].state = CPU_STATE_IDLE;
            }
        } else {
            cpu_data[cpuid].roles = ROLE_NONE;
            cpu_data[cpuid].state = CPU_STATE_UNKNOWN;
        }
        cpu_data[cpuid].wanted_for_parallel = false;
    }
    memcpy(&active_mask, &primary_thread_mask, sizeof(cpu_set_t));
    verbose(VB_OMPT, "Primary thread mask: %s", mu_to_str(&primary_thread_mask));
    verbose(VB_OMPT, "Worker threads mask: %s", mu_to_str(&worker_threads_mask));

    /* Initialize free agent lists */
    free_agent_lists_init();

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

        omptm_free_agents__lend();
    }
}

void omptm_free_agents__finalize(void) {
    /* Destrou CPU data */
    free(cpu_data);
    cpu_data = NULL;

    /* Destroy free agent lists and lock */
    free_agent_lists_destroy();
}


/*********************************************************************************/
/*  Blocking calls specific functions                                           */
/*********************************************************************************/

void omptm_free_agents__IntoBlockingCall(void) {
    if (lewi) {

        /* Don't know what to do if a Blocking Call is invoked inside a
         * parallel region. We could ignore it, but then we should also ignore
         * the associated OutOfBlockingCall, and how would we know it? */
        fatal_cond(DLB_ATOMIC_LD(&in_parallel),
                "Blocking Call inside a parallel region not supported");

        /* Warning: the current CPU, hopefully the one assigned to the primary
         * thread, has already been lent in the appropriate LeWI function
         * for the IntoBlockingCall event. */

        cpu_set_t cpus_to_lend;
        CPU_ZERO(&cpus_to_lend);

        /* Lend all CPUs not being used by workers or free agents */
        /* All owned CPUs (except primary) go from IDLE to LENT */
        int cpuid;
        for (cpuid=0; cpuid<system_size; ++cpuid) {
            if (cpu_data[cpuid].roles & (ROLE_WORKER | ROLE_FREE_AGENT)
                    && CPU_ISSET(cpuid, &process_mask)
                    && cas_bit((atomic_int*)&cpu_data[cpuid].state,
                        CPU_STATE_IDLE, CPU_STATE_LENT)) {
                CPU_SET(cpuid, &cpus_to_lend);
            }
        }
        DLB_LendCpuMask(&cpus_to_lend);
    }
}


void omptm_free_agents__OutOfBlockingCall(void) {
    if (lewi) {
        if (omptool_opts & OMPTOOL_OPTS_LEND) {
            /* Do nothing.
             * Do not reclaim since going out of a blocking call is not
             * an indication that the CPUs may be needed. */
        }
        else {
            cpu_set_t cpus_to_reclaim;
            CPU_ZERO(&cpus_to_reclaim);

            /* All owned CPUs (except primary) go from LENT to RECLAIMED */
            int cpuid;
            for (cpuid=0; cpuid<system_size; ++cpuid) {
                if (cpu_data[cpuid].roles & (ROLE_WORKER | ROLE_FREE_AGENT)
                        && CPU_ISSET(cpuid, &process_mask)
                        && cas_bit((atomic_int*)&cpu_data[cpuid].state,
                            CPU_STATE_LENT, CPU_STATE_RECLAIMED)) {
                    CPU_SET(cpuid, &cpus_to_reclaim);
                }
            }
            DLB_ReclaimCpuMask(&cpus_to_reclaim);
        }
    }
}


/*********************************************************************************/
/*  OMPT registered callbacks                                                    */
/*********************************************************************************/

void omptm_free_agents__thread_begin(ompt_thread_t thread_type) {
    /* Set up thread local spd */
    spd_enter_dlb(thread_spd);

    if (thread_type == ompt_thread_other) {
        int thread_id = __kmp_get_free_agent_id();
        fatal_cond(thread_id < 0, "free agent id < 0");
        __free_agent_id = thread_id;

        int cpuid = get_free_agent_binding(thread_id);

        if (cpuid >=0) {
            /* Set up free agent in free agent lists */
            free_agent_lists_register(thread_id, cpuid);

            /* Set up new CPU role */
            /* FIXME: not atomic */
            cpu_data[cpuid].roles |= ROLE_FREE_AGENT;

            /* TODO: set up cpu state? */

            /* Free agent threads start in disabled status */
            __kmp_set_free_agent_thread_active_status(thread_id, false);

            /* Bind free agent thread to cpuid */
            cpu_set_t thread_mask;
            CPU_ZERO(&thread_mask);
            CPU_SET(cpuid, &thread_mask);
            int err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            verbose(VB_OMPT, "Detected free agent thread %d. Pinning to CPU: %d, err: %d",
                    thread_id, cpuid, err);
        } else {
            warning("Could not find a suitable CPU bind for free agent thread id: %d",
                    thread_id);
        }
    }
}

void omptm_free_agents__parallel_begin(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        DLB_ATOMIC_ST(&in_parallel, true);

        /* Only if requested_parallelism == process_mask, reclaim all our lent CPUs, if needed */
        /* Otherwise, each thread will be responsible for reclaiming themselves */
        if (parallel_data->requested_parallelism == (unsigned)CPU_COUNT(&process_mask)) {
            int cpus_to_reclaim = 0;
            int cpuid_to_reclaim = -1;
            cpu_set_t mask_to_reclaim;
            int cpuid;
            for (cpuid = 0; cpuid<system_size; ++cpuid) {
                if (CPU_ISSET(cpuid, &process_mask)) {
                    DLB_ATOMIC_ST_RLX(&cpu_data[cpuid].wanted_for_parallel, true);

                    /* It is safe to just make a copy, we'll either call cb_disable_cpu
                     * which will do an atomic exchange or we'll call LeWI */
                    cpu_state_t cpu_state = DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].state);

                    if (cpu_state & CPU_STATE_FREE_AGENT_ENABLED) {
                        // Disable free agent thread from this CPU
                        cb_disable_cpu(cpuid, NULL);
                    }
                    else if (cpu_state & CPU_STATE_LENT) {
                        // Reclaim this CPU to LeWI
                        switch(++cpus_to_reclaim) {
                            case 1:
                                cpuid_to_reclaim = cpuid;
                                break;
                            case 2:
                                CPU_ZERO(&mask_to_reclaim);
                                CPU_SET(cpuid_to_reclaim, &mask_to_reclaim);
                                DLB_FALLTHROUGH;
                            default:
                                CPU_SET(cpuid, &mask_to_reclaim);
                        }
                    }
                }
            }
            if (cpus_to_reclaim == 1) {
                DLB_ReclaimCpu(cpuid_to_reclaim);
            } else if (cpus_to_reclaim > 1) {
                DLB_ReclaimCpuMask(&mask_to_reclaim);
            }
        }
    }
}

void omptm_free_agents__parallel_end(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        DLB_ATOMIC_ST(&in_parallel, false);
        /* All workers in parallel go to IDLE */
        int cpuid;
        for (cpuid=0; cpuid<system_size; ++cpuid) {
            if (cpu_data[cpuid].roles & ROLE_WORKER) {
                cas_bit((atomic_int*)&cpu_data[cpuid].state,
                        CPU_STATE_IN_PARALLEL, CPU_STATE_IDLE);
                DLB_ATOMIC_ST_RLX(&cpu_data[cpuid].wanted_for_parallel, false);
            }
        }

        omptm_free_agents__lend();
    }
}

void omptm_free_agents__into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index) {
    if (parallel_data->level == 1) {
        /* Obtain CPU id */
        /* FIXME: actually, we should test CPU binding every time we enter
            * here, since the RT is free to rebind threads, but we need
            * __worker_binding for testing */
        int cpuid = __worker_binding;
        if (cpuid == -1) {
            cpu_set_t thread_mask;
            pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            cpuid = mu_get_single_cpu(&thread_mask);
            fatal_cond(cpuid == -1,
                    "DLB does not currently support thread binding to more than one CPU,"
                    " current CPU affinity mask for thread %d: %s."
                    " Please, define OMP_PLACES=threads and run again.",
                    index, mu_to_str(&thread_mask));
            __worker_binding = cpuid;
        }


        /* Reclaim CPU if needed and set the appropriate state */
        if (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].state) & CPU_STATE_LENT) {
            DLB_ReclaimCpu(cpuid);
        }
        set_bit((atomic_int*)&cpu_data[cpuid].state, CPU_STATE_IN_PARALLEL);
        verbose(VB_OMPT, "CPU %d starting an implicit task", cpuid);
    }
}

void omptm_free_agents__task_create(void) {
    /* Increment the amount of pending tasks */
    DLB_ATOMIC_ADD(&pending_tasks, 1);

    /* For now, let's assume that we always want to increase the number
     * of active threads whenever a task is created
     */
    acquire_one_free_agent();
}

void omptm_free_agents__task_complete(void) {
    if (__free_agent_id >= 0) {
        int cpuid = get_free_agent_cpuid_by_id(__free_agent_id);

        /* Disable free agent thread if this CPU is needed for a worker thread */
        if (DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].state) & CPU_STATE_IN_PARALLEL
                || DLB_ATOMIC_LD_RLX(&cpu_data[cpuid].wanted_for_parallel)) {
            cb_disable_cpu(cpuid, NULL);
        }

        /* Return CPU if reclaimed */
        else if (DLB_CheckCpuAvailability(cpuid) == DLB_ERR_PERM) {
            if (DLB_ReturnCpu(cpuid) == DLB_ERR_PERM) {
                cb_disable_cpu(cpuid, NULL);
            }
        }

        /* Lend CPU if no more tasks */
        else if (DLB_ATOMIC_LD(&pending_tasks) == 0) {
            cb_disable_cpu(cpuid, NULL);

            /* Lend only free agents not part of the process mask */
            /* or, lend anyway if LEND policy */
            if (!CPU_ISSET(cpuid, &process_mask)
                    || omptool_opts & OMPTOOL_OPTS_LEND) {
                DLB_LendCpu(cpuid);
            }
        }
    }
}

void omptm_free_agents__task_switch(void) {
    if (DLB_ATOMIC_SUB(&pending_tasks, 1) > 1) {
        acquire_one_free_agent();
    }
}


/*********************************************************************************/
/*    Functions for testing purposes                                             */
/*********************************************************************************/

void omptm_free_agents_testing__set_worker_binding(int cpuid) {
    __worker_binding = cpuid;
}

void omptm_free_agents_testing__set_free_agent_id(int id) {
    __free_agent_id = id;
}

void omptm_free_agents_testing__set_pending_tasks(unsigned int num_tasks) {
    pending_tasks = num_tasks;
}

void omptm_free_agents_testing__acquire_one_free_agent(void) {
    acquire_one_free_agent();
}

bool omptm_free_agents_testing__in_parallel(void) {
    return DLB_ATOMIC_LD(&in_parallel);
}

bool omptm_free_agents_testing__check_cpu_in_parallel(int cpuid) {
    return DLB_ATOMIC_LD(&cpu_data[cpuid].state) & CPU_STATE_IN_PARALLEL;
}

bool omptm_free_agents_testing__check_cpu_idle(int cpuid) {
    return DLB_ATOMIC_LD(&cpu_data[cpuid].state) & CPU_STATE_IDLE;
}

bool omptm_free_agents_testing__check_cpu_free_agent_enabled(int cpuid) {
    return DLB_ATOMIC_LD(&cpu_data[cpuid].state) & CPU_STATE_FREE_AGENT_ENABLED;
}

int omptm_free_agents_testing__get_num_enabled_free_agents(void) {
    return DLB_ATOMIC_LD(&num_enabled_free_agents);
}

int omptm_free_agents_testing__get_free_agent_cpu(int thread_id) {
    return free_agent_cpu_list[thread_id];
}

int omptm_free_agents_testing__get_free_agent_binding(int thread_id) {
    return get_free_agent_binding(thread_id);
}

int omptm_free_agents_testing__get_free_agent_id_by_cpuid(int cpuid) {
    return get_free_agent_id_by_cpuid(cpuid);
}

int omptm_free_agents_testing__get_free_agent_cpuid_by_id(int thread_id) {
    return get_free_agent_cpuid_by_id(thread_id);
}
