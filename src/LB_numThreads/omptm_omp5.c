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

#include "LB_numThreads/omptm_omp5.h"

#include "apis/dlb.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/omptool.h"

#include <limits.h>
#include <sched.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

void omp_set_num_threads(int nthreads) __attribute__((weak));
static void (*set_num_threads_fn)(int) = NULL;


/*********************************************************************************/
/*  OMP Thread Manager                                                           */
/*********************************************************************************/

typedef enum openmp_places_t {
    OPENMP_PLACE_OTHER,
    OPENMP_PLACE_CORES,
    OPENMP_PLACE_THREADS,
} openmp_places_t;

static cpu_set_t active_mask, process_mask;
static bool lewi = false;
static bool drom = false;
static pid_t pid;
static int num_omp_threads_per_core;
static int hwthreads_per_core;
static int num_cores_in_process_mask;
static int num_initial_omp_threads;
static int num_initial_omp_threads_level1;
static omptool_opts_t omptool_opts;
static array_cpuid_t cpu_bindings;      /* available CPUs sorted by thread_num */

/* Based on the process_mask, active_mask and num_omp_threads_per_core, compute
 * the CPU binding for each thread number up to the system's highest CPU id */
static void compute_cpu_bindings(void) {

    /* clear previous bindings */
    array_cpuid_t_clear(&cpu_bindings);

    /* Iterate active mask and add potential CPUs to which threads may be bound to */
    for (int cpuid = mu_get_first_cpu(&active_mask);
            cpuid >= 0;
            cpuid = mu_get_cpu_next_core(&active_mask, cpuid)) {

        /* Add as many CPUs in the core as num_omp_threads_per_core */
        int num_added_cpus = 0;
        const mu_cpuset_t *core_mask = mu_get_core_mask(cpuid);
        for(int cpuid_in_core = core_mask->first_cpuid;
                cpuid_in_core >= 0;
                cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {

            /* Add CPU id as potentially available */
            array_cpuid_t_push(&cpu_bindings, cpuid_in_core);

            /* Stop adding if we reach the limit */
            if (++num_added_cpus == num_omp_threads_per_core) break;
        }
    }

    /* Sort list by ownership */
    qsort_r(cpu_bindings.items, cpu_bindings.count, sizeof(cpuid_t),
        mu_cmp_cpuids_by_ownership, &process_mask);
}

static void set_num_threads_from_active_mask(void) {
    if (num_omp_threads_per_core == hwthreads_per_core) {
        set_num_threads_fn(CPU_COUNT(&active_mask));
    } else {
        cpu_set_t active_core_set;
        mu_get_cores_subset_of_cpuset(&active_core_set, &active_mask);
        int num_active_cores = CPU_COUNT(&active_core_set) / hwthreads_per_core;
        set_num_threads_fn(num_active_cores * num_omp_threads_per_core);
    }
}

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
    set_num_threads_from_active_mask();
    compute_cpu_bindings();
}

static void cb_enable_cpu_set(const cpu_set_t *cpu_set, void *arg) {
    CPU_OR(&active_mask, &active_mask, cpu_set);
    set_num_threads_from_active_mask();
    compute_cpu_bindings();
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &active_mask);
    set_num_threads_from_active_mask();
    compute_cpu_bindings();
}

static void cb_disable_cpu_set(const cpu_set_t *cpu_set, void *arg) {
    mu_substract(&active_mask, &active_mask, cpu_set);
    set_num_threads_from_active_mask();
    compute_cpu_bindings();
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    memcpy(&process_mask, mask, sizeof(cpu_set_t));
    memcpy(&active_mask, mask, sizeof(cpu_set_t));

    /* Update number of cores in process mask */
    cpu_set_t core_set;
    mu_get_cores_subset_of_cpuset(&core_set, &process_mask);
    num_cores_in_process_mask = CPU_COUNT(&core_set) / hwthreads_per_core;

    set_num_threads_from_active_mask();
    compute_cpu_bindings();
}

static void omptm_omp5__borrow(void) {
    /* The "Exponential Weighted Moving Average" is an average computed
     * with weighting factors that decrease exponentially on new samples.
     * I.e,: Most recent values are more significant for the average */
    typedef struct ExponentialWeightedMovingAverage {
        float value;
        unsigned int samples;
    } ewma_t;
    static ewma_t ewma = {1.0f, 1};
    static int cpus_to_borrow = 1;

    if (drom) {
        DLB_PollDROM_Update();
    }

    if (lewi && omptool_opts & OMPTOOL_OPTS_BORROW) {
        int err_return = DLB_Return();
        int err_reclaim = DLB_Reclaim();
        int err_borrow = DLB_BorrowCpus(cpus_to_borrow);

        if (err_return == DLB_SUCCESS
                || err_reclaim == DLB_SUCCESS
                || err_borrow == DLB_SUCCESS) {
            verbose(VB_OMPT, "Acquire - Setting new mask to %s", mu_to_str(&active_mask));
        }

        if (err_borrow == DLB_SUCCESS) {
            cpus_to_borrow += ewma.value;
        } else {
            cpus_to_borrow -= ewma.value;
            cpus_to_borrow = max_int(1, cpus_to_borrow);
        }

        /* Update Exponential Weighted Moving Average */
        ++ewma.samples;
        float alpha = 2.0f / (ewma.samples + 1);
        ewma.value = ewma.value + alpha * (cpus_to_borrow - ewma.value);
    }
}

static void omptm_omp5__lend(void) {
    if (lewi && omptool_opts & OMPTOOL_OPTS_LEND) {
        set_num_threads_fn(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        compute_cpu_bindings();
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_SetMaxParallelism(1);
    }
}

void omptm_omp5__lend_from_api(void) {
    if (lewi) {
        set_num_threads_fn(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        compute_cpu_bindings();
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        //DLB_SetMaxParallelism(1);
    }
}


/* Parse OMP_PLACES. For now we are only interested if the variable is equal to
 * 'cores' or 'threads' */
static openmp_places_t parse_omp_places(void) {
    const char *env_places = getenv("OMP_PLACES");
    if (env_places) {
        if (strcmp(env_places, "cores") == 0) {
            return OPENMP_PLACE_CORES;
        } else if (strcmp(env_places, "threads") == 0) {
            return OPENMP_PLACE_THREADS;
        }
    }
    return OPENMP_PLACE_OTHER;
}

/* Parse OMP_NUM_THREADS and return the product of the values in the list
 * e.g.: OMP_NUM_THREADS=8 and OMP_NUM_THREADS=2,4 return 8 */
static int parse_omp_num_threads(int level) {
    const char *env_num_threads = getenv("OMP_NUM_THREADS");
    if (env_num_threads == NULL) return 0;

    int current_level = 0;
    int num_threads = 0;
    size_t len = strlen(env_num_threads) + 1;
    char *env_copy = malloc(sizeof(char)*len);
    strcpy(env_copy, env_num_threads);
    char *end = NULL;
    char *token = strtok_r(env_copy, ",", &end);
    while(token && current_level != level) {
        int num = strtol(token, NULL, 10);
        if (num > 0) {
            num_threads = num_threads == 0 ? num : num_threads * num;
        }
        token = strtok_r(NULL, ",", &end);
        ++current_level;
    }

    free(env_copy);

    return num_threads;
}

void omptm_omp5__init(pid_t process_id, const options_t *options) {
    if (omp_set_num_threads) {
        set_num_threads_fn = omp_set_num_threads;
    } else {
        void *handle = dlopen("libomp.so", RTLD_LAZY | RTLD_GLOBAL);
        if (handle == NULL) {
            handle = dlopen("libiomp5.so", RTLD_LAZY | RTLD_GLOBAL);
        }
        if (handle == NULL) {
            handle = dlopen("libgomp.so", RTLD_LAZY | RTLD_GLOBAL);
        }
        if (handle != NULL)  {
            set_num_threads_fn = dlsym(handle, "omp_set_num_threads");
        }
        fatal_cond(set_num_threads_fn == NULL, "omp_set_num_threads cannot be found");
    }

    lewi = options->lewi;
    drom = options->drom;
    omptool_opts = options->lewi_ompt;
    pid = process_id;
    hwthreads_per_core = mu_get_system_hwthreads_per_core();
    if (lewi || drom) {
        int err;
        err = DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet enable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_enable_cpu_set, (dlb_callback_t)cb_enable_cpu_set, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet enable_cpu_set: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet disable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_disable_cpu_set, (dlb_callback_t)cb_disable_cpu_set, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet disable_cpu_set: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_set_process_mask,
                (dlb_callback_t)cb_set_process_mask, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet set_process_mask: %s", DLB_Strerror(err));
        }

        /* Get process mask */
        shmem_procinfo__getprocessmask(pid, &process_mask, 0);
        memcpy(&active_mask, &process_mask, sizeof(cpu_set_t));
        verbose(VB_OMPT, "Initial mask set to: %s", mu_to_str(&process_mask));

        /* Update number of cores in process mask */
        cpu_set_t core_set;
        mu_get_cores_subset_of_cpuset(&core_set, &process_mask);
        num_cores_in_process_mask = CPU_COUNT(&core_set) / hwthreads_per_core;

        /* Best effort trying to compute OMP threads per core */
        openmp_places_t openmp_places = parse_omp_places();
        num_initial_omp_threads = parse_omp_num_threads(INT_MAX);
        num_initial_omp_threads_level1 = parse_omp_num_threads(1);
        if (openmp_places == OPENMP_PLACE_CORES) {
            num_omp_threads_per_core = 1;
        } else if (openmp_places == OPENMP_PLACE_THREADS) {
            num_omp_threads_per_core = hwthreads_per_core;
        } else if ( num_initial_omp_threads > 0) {
            num_omp_threads_per_core = max_int(
                1, num_initial_omp_threads / num_cores_in_process_mask);
        } else {
            num_omp_threads_per_core = 1;
        }
        verbose(VB_OMPT, "hwthreads per core: %d, omp threads per core: %d",
                hwthreads_per_core, num_omp_threads_per_core);

        array_cpuid_t_init(&cpu_bindings, 8);
        compute_cpu_bindings();

        omptm_omp5__lend();
    }
}

void omptm_omp5__finalize(void) {
}


/* lb_funcs.into_blocking_call has already been called and
 * the current CPU will be lent according to the --lew-mpi option
 * This function just lends the rest of the CPUs
 */
void omptm_omp5__IntoBlockingCall(void) {
    if (lewi) {
        int mycpu = sched_getcpu();

        /* Lend every CPU except the current one */
        cpu_set_t mask;
        memcpy(&mask, &active_mask, sizeof(cpu_set_t));
        CPU_CLR(mycpu, &mask);
        DLB_LendCpuMask(&mask);

        /* Set active_mask to only the current CPU */
        CPU_ZERO(&active_mask);
        CPU_SET(mycpu, &active_mask);
        set_num_threads_fn(1);
        compute_cpu_bindings();

        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}

void omptm_omp5__OutOfBlockingCall(void) {
    if (lewi) {
        DLB_Reclaim();

        /* Return to the initial number of threads */
        set_num_threads_fn(num_initial_omp_threads_level1);
    }
}


void omptm_omp5__parallel_begin(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        omptm_omp5__borrow();
    }
}

void omptm_omp5__parallel_end(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        omptm_omp5__lend();
    }
}

static inline int get_thread_binding(int index) {
    return cpu_bindings.items[index % cpu_bindings.count];
}

void omptm_omp5__into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index) {
    if (parallel_data->level == 1) {
        int cpuid = get_thread_binding(index);
        int current_cpuid = sched_getcpu();
        if (cpuid >=0 && cpuid != current_cpuid) {
            cpu_set_t thread_mask;
            CPU_ZERO(&thread_mask);
            CPU_SET(cpuid, &thread_mask);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            instrument_event(REBIND_EVENT, cpuid+1, EVENT_BEGIN);
            verbose(VB_OMPT, "Rebinding thread %d to CPU %d", index, cpuid);
        }
    }
}

void omptm_omp5__into_parallel_implicit_barrier(omptool_parallel_data_t *parallel_data) {
    if (parallel_data->level == 1) {
        /* A thread participating in the level 1 parallel region
         * has reached the implicit barrier */
        instrument_event(REBIND_EVENT,   0, EVENT_END);
    }
}


/*********************************************************************************/
/*    Functions for testing purposes                                             */
/*********************************************************************************/

const cpu_set_t* omptm_omp5_testing__get_active_mask(void) {
    return &active_mask;
}

const array_cpuid_t* omptm_omp5_testing__compute_and_get_cpu_bindings(void) {

    compute_cpu_bindings();
    return &cpu_bindings;
}

void omptm_omp5_testing__set_num_threads_fn(void (*fn)(int)) {
    set_num_threads_fn = fn;
}
