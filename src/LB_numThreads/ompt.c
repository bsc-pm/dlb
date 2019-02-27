/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "LB_numThreads/ompt.h"

#include "apis/dlb.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "apis/dlb_errors.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>

int omp_get_thread_num(void) __attribute__((weak));
void omp_set_num_threads(int nthreads) __attribute__((weak));
int omp_get_level(void) __attribute__((weak));


/*********************************************************************************/
/*  OMP Thread Manager                                                           */
/*********************************************************************************/

static cpu_set_t active_mask, process_mask;
static bool lewi = false;
static bool ompt = false;
static pid_t pid;
static ompt_opts_t ompt_opts;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
    omp_set_num_threads(CPU_COUNT(&active_mask));
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &active_mask);
    omp_set_num_threads(CPU_COUNT(&active_mask));
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    memcpy(&process_mask, mask, sizeof(cpu_set_t));
    memcpy(&active_mask, mask, sizeof(cpu_set_t));
    omp_set_num_threads(CPU_COUNT(&active_mask));
}

static void omp_thread_manager__borrow(void) {
    static int cpus_to_borrow = 1;

    if (lewi && ompt_opts & OMPT_OPTS_BORROW) {
        int err_return = DLB_Return();
        int err_reclaim = DLB_Reclaim();
        int err_borrow = DLB_BorrowCpus(cpus_to_borrow);

        if (err_return == DLB_SUCCESS
                || err_reclaim == DLB_SUCCESS
                || err_borrow == DLB_SUCCESS) {
            verbose(VB_OMPT, "Acquire - Setting new mask to %s", mu_to_str(&active_mask));
        }

        if (err_borrow == DLB_SUCCESS) {
            ++cpus_to_borrow;
        } else {
            cpus_to_borrow = 1;
        }

    }
}

static void omp_thread_manager__lend(void) {
    if (lewi && ompt_opts & OMPT_OPTS_LEND) {
        omp_set_num_threads(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_SetMaxParallelism(1);
    }
}

/* lb_funcs.into_blocking_call has already been called and
 * the current CPU will be lent according to the --lew-mpi option
 * This function just lends the rest of the CPUs
 */
void ompt_thread_manager__IntoBlockingCall(void) {
    if (lewi && ompt_opts & OMPT_OPTS_MPI) {
        int mycpu = sched_getcpu();

        /* Lend every CPU except the current one */
        cpu_set_t mask;
        memcpy(&mask, &active_mask, sizeof(cpu_set_t));
        CPU_CLR(mycpu, &mask);
        DLB_LendCpuMask(&mask);

        /* Set active_mask to only the current CPU */
        CPU_ZERO(&active_mask);
        CPU_SET(mycpu, &active_mask);
        omp_set_num_threads(1);

        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}

void ompt_thread_manager__OutOfBlockingCall(void) {
    if (lewi && ompt_opts & OMPT_OPTS_MPI) {
        DLB_Reclaim();
    }
}

static void omp_thread_manager__init(void) {
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
        shmem_procinfo__getprocessmask(pid, &process_mask, 0);
        memcpy(&active_mask, &process_mask, sizeof(cpu_set_t));
        verbose(VB_OMPT, "Initial mask set to: %s", mu_to_str(&process_mask));
        omp_thread_manager__lend();
    }
}

static void omp_thread_manager__finalize(void) {
}


/*********************************************************************************/
/*  OMPT callbacks                                                               */
/*********************************************************************************/

static void cb_parallel_begin(
        ompt_data_t *encountering_task_data,
        const omp_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_team_size,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager__borrow();
    }
}

static void cb_parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager__lend();
    }
}

static void cb_implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int team_size,
        unsigned int thread_num) {
    if (endpoint == ompt_scope_begin) {
        int cpuid = shmem_cpuinfo__get_thread_binding(pid, thread_num);
        int current_cpuid = sched_getcpu();
        if (cpuid >=0 && cpuid != current_cpuid) {
            cpu_set_t thread_mask;
            CPU_ZERO(&thread_mask);
            CPU_SET(cpuid, &thread_mask);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
            add_event(REBIND_EVENT, cpuid+1);
            verbose(VB_OMPT, "Rebinding thread %d to CPU %d", thread_num, cpuid);
        }
        add_event(BINDINGS_EVENT, sched_getcpu()+1);
    } else if (endpoint == ompt_scope_end) {
        add_event(REBIND_EVENT,   0);
        add_event(BINDINGS_EVENT, 0);
    }
}

static void cb_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
}

static void cb_thread_end(
        ompt_data_t *thread_data) {
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static inline int set_ompt_callback(ompt_set_callback_t set_callback_fn,
        ompt_callbacks_t which, ompt_callback_t callback) {
    int error = 1;
    switch (set_callback_fn(which, callback)) {
        case ompt_set_error:
            verbose(VB_OMPT, "OMPT callback %d failed", which);
            break;
        case ompt_set_never:
            verbose(VB_OMPT, "OMPT callback %d registered but will never be called", which);
            break;
        default:
            error = 0;
    }
    return error;
}

static int ompt_initialize(ompt_function_lookup_t ompt_fn_lookup, ompt_data_t *tool_data) {
    /* Parse options and get the required fields */
    options_t options;
    options_init(&options, NULL);
    lewi = options.lewi;
    ompt = options.ompt;
    ompt_opts = options.lewi_ompt;
    pid = options.preinit_pid ? options.preinit_pid : getpid();

    /* Enable experimental OMPT only if requested */
    if (ompt) {
        /* Initialize DLB only if ompt is enabled, otherwise DLB_Finalize won't be called */
        int err = DLB_Init(0, NULL, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_Init: %s", DLB_Strerror(err));
        }

        verbose(VB_OMPT, "Initializing OMPT module");

        ompt_set_callback_t set_callback_fn =
            (ompt_set_callback_t)ompt_fn_lookup("ompt_set_callback");
        if (set_callback_fn) {
            err = 0;
            err += set_ompt_callback(set_callback_fn,
                    ompt_callback_thread_begin,   (ompt_callback_t)cb_thread_begin);
            err += set_ompt_callback(set_callback_fn,
                    ompt_callback_thread_end,     (ompt_callback_t)cb_thread_end);
            err += set_ompt_callback(set_callback_fn,
                    ompt_callback_parallel_begin, (ompt_callback_t)cb_parallel_begin);
            err += set_ompt_callback(set_callback_fn,
                    ompt_callback_parallel_end,   (ompt_callback_t)cb_parallel_end);
            err += set_ompt_callback(set_callback_fn,
                    ompt_callback_implicit_task,  (ompt_callback_t)cb_implicit_task);

            if (!err) {
                verbose(VB_OMPT, "OMPT callbacks succesfully registered");
            }

            omp_thread_manager__init();
        } else {
            verbose(VB_OMPT, "Could not look up function \"ompt_set_callback\"");
        }

        // return a non-zero value to activate the tool
        return 1;
    }

    return 0;
}

static void ompt_finalize(ompt_data_t *tool_data) {
    if (ompt) {
        verbose(VB_OMPT, "Finalizing OMPT module");
        omp_thread_manager__finalize();
        DLB_Finalize();
    }
}


#pragma GCC visibility push(default)
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    static ompt_start_tool_result_t ompt_start_tool_result = {
        .initialize = ompt_initialize,
        .finalize   = ompt_finalize,
        .tool_data  = {0}
    };
    return &ompt_start_tool_result;
}
#pragma GCC visibility pop

/*********************************************************************************/

#ifdef DEBUG_VERSION
/* Static type checking */
static __attribute__((unused)) void ompt_type_checking(void) {
    { ompt_initialize_t                 __attribute__((unused)) fn = ompt_initialize; }
    { ompt_finalize_t                   __attribute__((unused)) fn = ompt_finalize; }
    { ompt_callback_thread_begin_t      __attribute__((unused)) fn = cb_thread_begin; }
    { ompt_callback_thread_end_t        __attribute__((unused)) fn = cb_thread_end; }
    { ompt_callback_parallel_begin_t    __attribute__((unused)) fn = cb_parallel_begin; }
    { ompt_callback_parallel_end_t      __attribute__((unused)) fn = cb_parallel_end; }
    { ompt_callback_implicit_task_t     __attribute__((unused)) fn = cb_implicit_task; }
}
#endif
