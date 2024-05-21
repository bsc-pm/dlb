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

/* Configure second tool with:
 *  DLB_TOOL_LIBRARIES
 *  DLB_TOOL_VERBOSE_INIT
 */
#define OMPT_MULTIPLEX_TOOL_NAME "DLB"
#include "LB_numThreads/ompt-multiplex.h"

#include "LB_numThreads/omptool.h"

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptm_omp5.h"
#include "LB_numThreads/omptm_free_agents.h"
#include "LB_numThreads/omptm_role_shift.h"
#include "LB_core/spd.h"
#include "apis/dlb.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>


static funcs_t talp_funcs = {0};
static funcs_t omptm_funcs = {0};
static funcs_t multiplex_funcs = {0};
static funcs_t *cb_funcs = NULL;
static ompt_set_callback_t set_callback_fn = NULL;


/*********************************************************************************/
/*  Init & Finalize                                                              */
/*********************************************************************************/

static void omptool__init(pid_t process_id, const options_t *options) {
    if (talp_funcs.init) {
        talp_funcs.init(process_id, options);
    }
    if (omptm_funcs.init) {
        omptm_funcs.init(process_id, options);
    }
}

static void omptool__finalize(void) {
    if (talp_funcs.finalize) {
        talp_funcs.finalize();
    }
    if (omptm_funcs.finalize) {
        omptm_funcs.finalize();
    }
}


/*********************************************************************************/
/*  "Callbacks" from MPI and DLB API                                             */
/*********************************************************************************/

void omptool__into_blocking_call(void) {
    if (talp_funcs.into_mpi) {
        talp_funcs.into_mpi();
    }
    if (omptm_funcs.into_mpi) {
        omptm_funcs.into_mpi();
    }
}

void omptool__outof_blocking_call(void) {
    if (omptm_funcs.outof_mpi) {
        omptm_funcs.outof_mpi();
    }
    if (talp_funcs.outof_mpi) {
        talp_funcs.outof_mpi();
    }
}

void omptool__lend_from_api(void) {
    if (omptm_funcs.lend_from_api) {
        omptm_funcs.lend_from_api();
    }
}


/*********************************************************************************/
/*  Multiplex functions, only needed if omptm and TALP are enabled               */
/*********************************************************************************/

static void multiplex__thread_begin(
        ompt_thread_t thread_type,
        ompt_data_t *thread_data) {
    if (talp_funcs.thread_begin) {
        talp_funcs.thread_begin(thread_type, thread_data);
    }
    if (omptm_funcs.thread_begin) {
        omptm_funcs.thread_begin(thread_type, thread_data);
    }
}

static void multiplex__thread_end(
        ompt_data_t *thread_data) {
    if (talp_funcs.thread_end) {
        talp_funcs.thread_end(thread_data);
    }
    if (omptm_funcs.thread_end) {
        omptm_funcs.thread_end(thread_data);
    }
}

static void multiplex__parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_parallelism,
        int flags,
        const void *codeptr_ra) {
    if (talp_funcs.parallel_begin) {
        talp_funcs.parallel_begin(encountering_task_data, encountering_task_frame,
                parallel_data, requested_parallelism, flags, codeptr_ra);
    }
    if (omptm_funcs.parallel_begin) {
        omptm_funcs.parallel_begin(encountering_task_data, encountering_task_frame,
                parallel_data, requested_parallelism, flags, codeptr_ra);
    }
}

static void multiplex__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (talp_funcs.parallel_end) {
        talp_funcs.parallel_end(parallel_data, encountering_task_data, flags, codeptr_ra);
    }
    if (omptm_funcs.parallel_end) {
        omptm_funcs.parallel_end(parallel_data, encountering_task_data, flags, codeptr_ra);
    }
}

static void multiplex__task_create(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *new_task_data,
        int flags,
        int has_dependences,
        const void *codeptr_ra) {
    if (talp_funcs.task_create) {
        talp_funcs.task_create(encountering_task_data, encountering_task_frame,
                new_task_data, flags, has_dependences, codeptr_ra);
    }
    if (omptm_funcs.task_create) {
        omptm_funcs.task_create(encountering_task_data, encountering_task_frame,
                new_task_data, flags, has_dependences, codeptr_ra);
    }
}

static void multiplex__task_schedule(
        ompt_data_t *prior_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *next_task_data) {
    if (talp_funcs.task_schedule) {
        talp_funcs.task_schedule(prior_task_data, prior_task_status, next_task_data);
    }
    if (omptm_funcs.task_schedule) {
        omptm_funcs.task_schedule(prior_task_data, prior_task_status, next_task_data);
    }
}

static void multiplex__implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int actual_parallelism,
        unsigned int index,
        int flags) {
    if (talp_funcs.implicit_task) {
        talp_funcs.implicit_task(endpoint, parallel_data, task_data,
                actual_parallelism, index, flags);
    }
    if (omptm_funcs.implicit_task) {
        omptm_funcs.implicit_task(endpoint, parallel_data, task_data,
                actual_parallelism, index, flags);
    }
}

static void multiplex__work(
        ompt_work_t wstype,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        uint64_t count,
        const void *codeptr_ra) {
    if (talp_funcs.work) {
        talp_funcs.work(wstype, endpoint, parallel_data, task_data, count, codeptr_ra);
    }
    if (omptm_funcs.work) {
        omptm_funcs.work(wstype, endpoint, parallel_data, task_data, count, codeptr_ra);
    }
}

static void multiplex__sync_region(
        ompt_sync_region_t kind,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        const void *codeptr_ra) {
    if (talp_funcs.sync_region) {
        talp_funcs.sync_region(kind, endpoint, parallel_data, task_data, codeptr_ra);
    }
    if (omptm_funcs.sync_region) {
        omptm_funcs.sync_region(kind, endpoint, parallel_data, task_data, codeptr_ra);
    }
}


/*********************************************************************************/
/*  Function pointers setup                                                      */
/*********************************************************************************/

/* For testing purposes only, predefine function pointers */
static bool test_funcs_init = false;
void omptool_testing__setup_omp_fn_ptrs(const funcs_t *talp_test_funcs,
        const funcs_t *omptm_test_funcs) {
    test_funcs_init = true;
    talp_funcs = *talp_test_funcs;
    omptm_funcs = *omptm_test_funcs;
}

static void setup_omp_fn_ptrs(omptm_version_t omptm_version, bool talp_openmp) {
    if (!test_funcs_init) {
        /* talp_funcs */
        if (talp_openmp) {
            verbose(VB_OMPT, "Enabling OMPT support for TALP");
            talp_funcs = (const funcs_t) {
                .init               = NULL,
                .finalize           = NULL,
                .into_mpi           = NULL,
                .outof_mpi          = NULL,
                .lend_from_api      = NULL,
                .thread_begin       = NULL,
                .thread_end         = NULL,
                .thread_role_shift  = NULL,
                .parallel_begin     = NULL,
                .parallel_end       = NULL,
                .task_create        = NULL,
                .task_schedule      = NULL,
                .implicit_task      = NULL,
                .work               = NULL,
                .sync_region        = NULL,
            };
        }

        /* omptm_funcs */
        if (omptm_version == OMPTM_OMP5) {
            omptm_funcs = (const funcs_t) {
                .init               = omptm_omp5__init,
                .finalize           = omptm_omp5__finalize,
                .into_mpi           = omptm_omp5__IntoBlockingCall,
                .outof_mpi          = omptm_omp5__OutOfBlockingCall,
                .lend_from_api      = omptm_omp5__lend_from_api,
                .thread_begin       = NULL,
                .thread_end         = NULL,
                .thread_role_shift  = NULL,
                .parallel_begin     = omptm_omp5__parallel_begin,
                .parallel_end       = omptm_omp5__parallel_end,
                .task_create        = NULL,
                .task_schedule      = omptm_omp5__task_schedule,
                .implicit_task      = omptm_omp5__implicit_task,
                .work               = NULL,
                .sync_region        = omptm_omp5__sync_region,
            };
        } else if (omptm_version == OMPTM_FREE_AGENTS) {
            verbose(VB_OMPT, "Enabling experimental support with free agent threads");
            omptm_funcs = (const funcs_t) {
                .init               = omptm_free_agents__init,
                .finalize           = omptm_free_agents__finalize,
                .into_mpi           = omptm_free_agents__IntoBlockingCall,
                .outof_mpi          = omptm_free_agents__OutOfBlockingCall,
                .lend_from_api      = NULL,
                .thread_begin       = omptm_free_agents__thread_begin,
                .thread_end         = NULL,
                .thread_role_shift  = NULL,
                .parallel_begin     = omptm_free_agents__parallel_begin,
                .parallel_end       = omptm_free_agents__parallel_end,
                .task_create        = omptm_free_agents__task_create,
                .task_schedule      = omptm_free_agents__task_schedule,
                .implicit_task      = omptm_free_agents__implicit_task,
                .work               = NULL,
                .sync_region        = omptm_free_agents__sync_region,
            };
        }
        else if (omptm_version == OMPTM_ROLE_SHIFT) {
            verbose(VB_OMPT, "Enabling experimental support with role shift threads");
            omptm_funcs = (const funcs_t) {
                .init               = omptm_role_shift__init,
                .finalize           = omptm_role_shift__finalize,
                .into_mpi           = omptm_role_shift__IntoBlockingCall,
                .outof_mpi          = omptm_role_shift__OutOfBlockingCall,
                .lend_from_api      = NULL,
                .thread_begin       = omptm_role_shift__thread_begin,
                .thread_end         = NULL,
                .thread_role_shift  = omptm_role_shift__thread_role_shift,
                .parallel_begin     = omptm_role_shift__parallel_begin,
                .parallel_end       = omptm_role_shift__parallel_end,
                .task_create        = omptm_role_shift__task_create,
                .task_schedule      = omptm_role_shift__task_schedule,
                .implicit_task      = NULL,
                .work               = NULL,
                .sync_region        = NULL,
            };
        }
    }

    /* multiplex_funcs */
    multiplex_funcs = (const funcs_t) {
        .thread_begin           = multiplex__thread_begin,
        .thread_end             = multiplex__thread_end,
        .parallel_begin         = multiplex__parallel_begin,
        .parallel_end           = multiplex__parallel_end,
        .task_create            = multiplex__task_create,
        .task_schedule          = multiplex__task_schedule,
        .implicit_task          = multiplex__implicit_task,
        .work                   = multiplex__work,
        .sync_region            = multiplex__sync_region,
    };

    /* Point to the appropriate struct */
    if (omptm_version != OMPTM_NONE) {
        if (talp_openmp) {
            cb_funcs = &multiplex_funcs;
        } else {
            cb_funcs = &omptm_funcs;
        }
    } else {
        if (talp_openmp) {
            cb_funcs = &talp_funcs;
        } else {
            cb_funcs = NULL;
        }
    }

    /* The following function is a custom callback and it's only used in the
     * experimental role-shift thread manager */
    if (omptm_version == OMPTM_ROLE_SHIFT) {
        cb_funcs->thread_role_shift = omptm_role_shift__thread_role_shift;
    }
}

static inline int set_ompt_callback(ompt_callbacks_t event, ompt_callback_t callback) {
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

static int set_ompt_callbacks(ompt_function_lookup_t lookup, omptm_version_t omptm_version,
        bool talp_openmp) {

    /* Populate global structs */
    setup_omp_fn_ptrs(omptm_version, talp_openmp);

    if (cb_funcs == NULL) {
        warning("OMPT is enabled (--ompt) but no thread manager was selected"
                " (--ompt-thread-manager) nor support for TALP was enabled"
                " (--talp-openmp). DLB will not register any OMPT callback");
        return 0;
    }

    int error = 0;
    set_callback_fn = (ompt_set_callback_t)lookup("ompt_set_callback");
    if (set_callback_fn) {
            if (cb_funcs->thread_begin) {
                error += set_ompt_callback(
                        ompt_callback_thread_begin,
                        (ompt_callback_t)cb_funcs->thread_begin);
            }
            if (cb_funcs->thread_end) {
                error += set_ompt_callback(
                        ompt_callback_thread_end,
                        (ompt_callback_t)cb_funcs->thread_end);
            }
            if (cb_funcs->thread_role_shift) {
                error += set_ompt_callback(
                        ompt_callback_thread_role_shift,
                        (ompt_callback_t)cb_funcs->thread_role_shift);
            }
            if (cb_funcs->parallel_begin) {
                error += set_ompt_callback(
                        ompt_callback_parallel_begin,
                        (ompt_callback_t)cb_funcs->parallel_begin);
            }
            if (cb_funcs->parallel_end) {
                error += set_ompt_callback(
                        ompt_callback_parallel_end,
                        (ompt_callback_t)cb_funcs->parallel_end);
            }
            if (cb_funcs->task_create) {
                error += set_ompt_callback(
                        ompt_callback_task_create,
                        (ompt_callback_t)cb_funcs->task_create);
            }
            if (cb_funcs->task_schedule) {
                error += set_ompt_callback(
                        ompt_callback_task_schedule,
                        (ompt_callback_t)cb_funcs->task_schedule);
            }
            if (cb_funcs->implicit_task) {
                error += set_ompt_callback(
                        ompt_callback_implicit_task,
                        (ompt_callback_t)cb_funcs->implicit_task);
            }
            if (cb_funcs->work) {
                error += set_ompt_callback(
                        ompt_callback_work,
                        (ompt_callback_t)cb_funcs->work);
            }
            if (cb_funcs->sync_region) {
                error += set_ompt_callback(
                        ompt_callback_sync_region,
                        (ompt_callback_t)cb_funcs->sync_region);
            }
    } else {
        error = 1;
        verbose(VB_OMPT, "Could not look up function \"ompt_set_callback\"");
    }

    if (!error) {
        verbose(VB_OMPT, "OMPT callbacks successfully registered");
    }

    return error;
}

static void unset_ompt_callbacks(void) {
    if (set_callback_fn) {
        if (cb_funcs->thread_begin) {
            set_callback_fn(ompt_callback_thread_begin, NULL);
        }
        if (cb_funcs->thread_end) {
            set_callback_fn(ompt_callback_thread_end, NULL);
        }
        if (cb_funcs->thread_role_shift) {
            set_callback_fn(ompt_callback_thread_role_shift, NULL);
        }
        if (cb_funcs->parallel_begin) {
            set_callback_fn(ompt_callback_parallel_begin, NULL);
        }
        if (cb_funcs->parallel_end) {
            set_callback_fn(ompt_callback_parallel_end, NULL);
        }
        if (cb_funcs->task_create) {
            set_callback_fn(ompt_callback_task_create, NULL);
        }
        if (cb_funcs->task_schedule) {
            set_callback_fn(ompt_callback_task_schedule, NULL);
        }
        if (cb_funcs->implicit_task) {
            set_callback_fn(ompt_callback_implicit_task, NULL);
        }
        if (cb_funcs->work) {
            set_callback_fn(ompt_callback_work, NULL);
        }
        if (cb_funcs->sync_region) {
            set_callback_fn(ompt_callback_sync_region, NULL);
        }
    }
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static bool dlb_initialized_through_ompt = false;
static const char *openmp_runtime_version;

static int omptool_initialize(ompt_function_lookup_t lookup, int initial_device_num,
        ompt_data_t *tool_data) {
    /* Parse options and get the required fields */
    options_t options;
    options_init(&options, NULL);
    debug_init(&options);

    /* Print OMPT version and variables*/
    const char *omp_policy_str = getenv("OMP_WAIT_POLICY");
    verbose(VB_OMPT, "Detected OpenMP runtime: %s", openmp_runtime_version);
    verbose(VB_OMPT, "Environment variables of interest:");
    verbose(VB_OMPT, "  OMP_WAIT_POLICY: %s", omp_policy_str);
    if (strncmp(openmp_runtime_version, "Intel", 5) == 0 ||
            strncmp(openmp_runtime_version, "LLVM", 4) == 0) {
        verbose(VB_OMPT, "  KMP_LIBRARY: %s", getenv("KMP_LIBRARY"));
        verbose(VB_OMPT, "  KMP_BLOCKTIME: %s", getenv("KMP_BLOCKTIME"));
    }
    /* when GCC implements OMPT: else if "gomp", print GOMP_SPINCOUNT */

    verbose(VB_OMPT, "DLB with OMPT support is %s", options.ompt ? "ENABLED" : "DISABLED");

    /* Enable OMPT only if requested */
    if (options.ompt) {
        /* Emit warning if OMP_WAIT_POLICY is not "passive" */
        if (options.lewi &&
                options.omptm_version == OMPTM_OMP5 &&
                (!omp_policy_str || strcasecmp(omp_policy_str, "passive") != 0)) {
            warning("OMP_WAIT_POLICY value it not \"passive\". Even though the default "
                    "value may be \"passive\", setting it explicitly is recommended "
                    "since it modifies other runtime related environment variables");
        }

        int err = 0;
        if (options.preinit_pid == 0) {
            /* Force init */
            cpu_set_t process_mask;
            sched_getaffinity(0, sizeof(process_mask), &process_mask);
            err = DLB_Init(0, &process_mask, NULL);
            if (err == DLB_SUCCESS) {
                dlb_initialized_through_ompt = true;
            } else if (err != DLB_ERR_INIT) {
                warning("DLB_Init failed: %s", DLB_Strerror(err));
            }
        } else {
            /* Initialize DLB only if ompt is enabled, and
            * remember if succeeded to finalize it when ompt_finalize is invoked. */
            err = DLB_Init(0, NULL, NULL);
            if (err == DLB_SUCCESS) {
                dlb_initialized_through_ompt = true;
            } else {
                verbose(VB_OMPT, "DLB_Init: %s", DLB_Strerror(err));
            }
        }

        verbose(VB_OMPT, "Initializing OMPT module");

        /* Register OMPT callbacks */
        err = set_ompt_callbacks(lookup, options.omptm_version, options.talp_openmp);

        /* If callbacks are successfully registered, initialize modules
         * and return a non-zero value to activate the tool */
        if (!err) {
            omptool__init(thread_spd->id, &options);
            options_finalize(&options);
            return 1;
        }

        /* Otherwise, finalize DLB if init succeeded */
        if (dlb_initialized_through_ompt) {
            DLB_Finalize();
        }

        warning("DLB could not register itself as OpenMP tool");
    }

    options_finalize(&options);

    return 0;
}

static void omptool_finalize(ompt_data_t *tool_data) {
    verbose(VB_OMPT, "Finalizing OMPT module");

    /* Finalize DLB if needed */
    if (dlb_initialized_through_ompt) {
        DLB_Finalize();
    }

    /* Disable OMPT callbacks */
    unset_ompt_callbacks();

    /* Finalize modules */
    omptool__finalize();
}


#pragma GCC visibility push(default)
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    openmp_runtime_version = runtime_version;
    static ompt_start_tool_result_t tool = {
        .initialize = omptool_initialize,
        .finalize   = omptool_finalize,
        .tool_data  = {0}
    };
    return &tool;
}
#pragma GCC visibility pop
