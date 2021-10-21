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

#include "LB_numThreads/ompt.h"

#include "LB_numThreads/omp_thread_manager.h"
#include "apis/dlb.h"
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

enum {
    PARALLEL_UNSET,
    PARALLEL_LEVEl_1,
};

static pid_t pid;

/*********************************************************************************/
/*  OMPT callbacks                                                               */
/*********************************************************************************/

static void cb_parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_parallelism,
        int flags,
        const void *codeptr_ra) {
    /*"The exit frame associated with the initial task that is not nested
     * inside any OpenMP construct is NULL."
     */
    if (encountering_task_frame->exit_frame.ptr == NULL
            && flags & ompt_parallel_team) {
        /* This is a non-nested parallel construct encountered by the initial task.
         * Set parallel_data to an appropriate value so that worker threads know
         * when they start their explicit task for this parallel region.
         */
        parallel_data->value = PARALLEL_LEVEl_1;

        omp_thread_manager__borrow();
    }
}

static void cb_parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (parallel_data->value == PARALLEL_LEVEl_1) {
        omp_thread_manager__lend();
        parallel_data->value = PARALLEL_UNSET;
    }
}

static void cb_implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int actual_parallelism,
        unsigned int index,
        int flags) {
    if (endpoint == ompt_scope_begin) {
        if (parallel_data &&
                parallel_data->value == PARALLEL_LEVEl_1) {
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

static void cb_thread_begin(
        ompt_thread_t thread_type,
        ompt_data_t *thread_data) {
}

static void cb_thread_end(
        ompt_data_t *thread_data) {
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static bool dlb_initialized_through_ompt = false;
static const char *omp_runtime_version;

static inline int set_ompt_callback(ompt_set_callback_t set_callback_fn,
        ompt_callbacks_t event, ompt_callback_t callback) {
    int error = 1;
    switch (set_callback_fn(event, callback)) {
        case ompt_set_error:
            verbose(VB_OMPT, "OMPT set callback %d failed.", event);
            break;
        case ompt_set_never:
            verbose(VB_OMPT, "OMPT set callback %d returned 'never'. The event was "
                    "registered but it will never occur or the callback will never "
                    "be invoked at runtime.", event);
            break;
        case ompt_set_impossible:
            verbose(VB_OMPT, "OMPT set callback %d returned 'impossible'. The event "
                    "may occur but the tracing of it is not possible.", event);
            break;
        case ompt_set_sometimes:
            verbose(VB_OMPT, "OMPT set callback %d returned 'sometimes'. The event "
                    "may occur and the callback will be invoked at runtime, but "
                    "only for an implementation-defined subset of associated event "
                    "occurrences.", event);
            error = 0;
            break;
        case ompt_set_sometimes_paired:
            verbose(VB_OMPT, "OMPT set callback %d returned 'sometimes paired'. "
                    "The event may occur and the callback will be invoked at "
                    "runtime, but only for an implementation-defined subset of "
                    "associated event occurrences. If any callback is invoked "
                    "with a begin_scope endpoint, it will be invoked also later "
                    "with and end_scope endpoint.", event);
            error = 0;
            break;
        case ompt_set_always:
            error = 0;
            break;
        default:
            fatal("Unsupported return code at set_ompt_callback, "
                    "please file a bug report.");
    }
    return error;
}

static int ompt_initialize(ompt_function_lookup_t lookup, int initial_device_num,
        ompt_data_t *tool_data) {
    /* Parse options and get the required fields */
    options_t options;
    options_init(&options, NULL);
    debug_init(&options);
    pid = getpid();

    /* Print OMPT version and variables*/
    const char *omp_policy_str = getenv("OMP_WAIT_POLICY");
    verbose(VB_OMPT, "Detected OpenMP runtime: %s", omp_runtime_version);
    verbose(VB_OMPT, "Environment variables of interest:");
    verbose(VB_OMPT, "  OMP_WAIT_POLICY: %s", omp_policy_str);
    if (strncmp(omp_runtime_version, "Intel", 5) == 0) {
        verbose(VB_OMPT, "  KMP_LIBRARY: %s", getenv("KMP_LIBRARY"));
        verbose(VB_OMPT, "  KMP_BLOCKTIME: %s", getenv("KMP_BLOCKTIME"));
    }
    /* when GCC 10 is relased: else if "gomp", print GOMP_SPINCOUNT */

    verbose(VB_OMPT, "DLB with OMPT support is %s", options.ompt ? "ENABLED" : "DISABLED");

    /* Enable OMPT only if requested */
    if (options.ompt) {
        /* Emit warning if OMP_WAIT_POLICY is not "passive" */
        if (options.lewi &&
                (!omp_policy_str || strcasecmp(omp_policy_str, "passive") != 0)) {
            warning("OMP_WAIT_POLICY value it not \"passive\". Even though the default "
                    "value may be \"passive\", setting it explicitly is recommended "
                    "since it modifies other runtime related environment variables");
        }

        /* Initialize DLB only if ompt is enabled, and
         * remember if succeded to finalize it when ompt_finalize is invoked. */
        int err = DLB_Init(0, NULL, NULL);
        if (err == DLB_SUCCESS) {
            dlb_initialized_through_ompt = true;
        } else {
            verbose(VB_OMPT, "DLB_Init: %s", DLB_Strerror(err));
        }

        verbose(VB_OMPT, "Initializing OMPT module");

        /* Register OMPT callbacks */
        ompt_set_callback_t set_callback_fn =
            (ompt_set_callback_t)lookup("ompt_set_callback");
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
        } else {
            err = 1;
            verbose(VB_OMPT, "Could not look up function \"ompt_set_callback\"");
        }

        /* If callbacks are successfully registered, initialize thread manager
         * and return a non-zero value to activate the tool */
        if (!err) {
            verbose(VB_OMPT, "OMPT callbacks succesfully registered");
            omp_thread_manager__init(&options);
            return 1;
        }

        /* Otherwise, finalize DLB if init succeeded */
        if (dlb_initialized_through_ompt) {
            DLB_Finalize();
        }

        warning("DLB could not register itself as OpenMP tool");
    }

    return 0;
}

static void ompt_finalize(ompt_data_t *tool_data) {
    if (dlb_initialized_through_ompt) {
        verbose(VB_OMPT, "Finalizing OMPT module");
        omp_thread_manager__finalize();
        DLB_Finalize();
    }
}


#pragma GCC visibility push(default)
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    omp_runtime_version = runtime_version;
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
