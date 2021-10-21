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

#include "LB_numThreads/omptm_omp5.h"

#include "apis/dlb.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"

#include <sched.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

void omp_set_num_threads(int nthreads) __attribute__((weak));
static void (*set_num_threads_fn)(int) = NULL;

enum {
    PARALLEL_UNSET,
    PARALLEL_LEVEL_1,
};


/*********************************************************************************/
/*  OMP Thread Manager                                                           */
/*********************************************************************************/

static cpu_set_t active_mask, process_mask;
static bool lewi = false;
static bool drom = false;
static pid_t pid;
static omptool_opts_t omptool_opts;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
    set_num_threads_fn(CPU_COUNT(&active_mask));
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &active_mask);
    set_num_threads_fn(CPU_COUNT(&active_mask));
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    memcpy(&process_mask, mask, sizeof(cpu_set_t));
    memcpy(&active_mask, mask, sizeof(cpu_set_t));
    set_num_threads_fn(CPU_COUNT(&active_mask));
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
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_SetMaxParallelism(1);
    }
}

void omptm_omp5__lend_from_api(void) {
    if (lewi) {
        set_num_threads_fn(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        //DLB_SetMaxParallelism(1);
    }
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
    if (lewi || drom) {
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
        shmem_procinfo__getprocessmask(pid, &process_mask, 0);
        memcpy(&active_mask, &process_mask, sizeof(cpu_set_t));
        verbose(VB_OMPT, "Initial mask set to: %s", mu_to_str(&process_mask));
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
    if (lewi && omptool_opts & OMPTOOL_OPTS_MPI) {
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

        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}

void omptm_omp5__OutOfBlockingCall(void) {
    if (lewi && omptool_opts & OMPTOOL_OPTS_MPI) {
        DLB_Reclaim();
    }
}


void omptm_omp5__parallel_begin(
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

        omptm_omp5__borrow();
    }
}

void omptm_omp5__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (parallel_data->value == PARALLEL_LEVEL_1) {
        omptm_omp5__lend();
        parallel_data->value = PARALLEL_UNSET;
    }
}

void omptm_omp5__implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int actual_parallelism,
        unsigned int index,
        int flags) {
    if (endpoint == ompt_scope_begin) {
        if (parallel_data &&
                parallel_data->value == PARALLEL_LEVEL_1) {
            int cpuid = shmem_cpuinfo__get_thread_binding(pid, index);
            int current_cpuid = sched_getcpu();
            if (cpuid >=0 && cpuid != current_cpuid) {
                cpu_set_t thread_mask;
                CPU_ZERO(&thread_mask);
                CPU_SET(cpuid, &thread_mask);
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
                instrument_event(REBIND_EVENT, cpuid+1, EVENT_BEGIN);
                verbose(VB_OMPT, "Rebinding thread %d to CPU %d", index, cpuid);
            }
            instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
        }
    } else if (endpoint == ompt_scope_end) {
        instrument_event(REBIND_EVENT,   0, EVENT_END);
        instrument_event(BINDINGS_EVENT, 0, EVENT_END);
    }
}
