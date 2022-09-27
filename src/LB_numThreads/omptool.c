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

#define CLIENT_TOOL_LIBRARIES_VAR "DLB_TOOL_LIBRARIES"
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

typedef struct {
    /* Init & Finalize function */
    void (*init)(pid_t, const options_t*);
    void (*finalize)(void);
    /* MPI calls */
    void (*into_mpi)(void);
    void (*outof_mpi)(void);
    /* Intercept Lend */
    void (*lend_from_api)(void);
    /* OMPT callbacks */
    ompt_callback_thread_begin_t      thread_begin;
    ompt_callback_thread_end_t        thread_end;
    ompt_callback_thread_role_shift_t thread_role_shift;
    ompt_callback_parallel_begin_t    parallel_begin;
    ompt_callback_parallel_end_t      parallel_end;
    ompt_callback_task_create_t       task_create;
    ompt_callback_task_schedule_t     task_schedule;
    ompt_callback_implicit_task_t     implicit_task;
    ompt_callback_work_t              work;
    ompt_callback_sync_region_t       sync_region;
} openmp_thread_manager_funcs_t;
static openmp_thread_manager_funcs_t omptm_funcs = {0};

static void setup_omp_fn_ptrs(omptm_version_t omptm_version) {
    if (omptm_version == OMPTM_OMP5) {
        omptm_funcs.init              = omptm_omp5__init;
        omptm_funcs.finalize          = omptm_omp5__finalize;
        omptm_funcs.into_mpi          = omptm_omp5__IntoBlockingCall;
        omptm_funcs.outof_mpi         = omptm_omp5__OutOfBlockingCall;
        omptm_funcs.lend_from_api     = omptm_omp5__lend_from_api;
        omptm_funcs.thread_begin      = NULL;
        omptm_funcs.thread_end        = NULL;
        omptm_funcs.thread_role_shift = NULL;
        omptm_funcs.parallel_begin    = omptm_omp5__parallel_begin;
        omptm_funcs.parallel_end      = omptm_omp5__parallel_end;
        omptm_funcs.task_create       = NULL;
        omptm_funcs.task_schedule     = NULL;
        omptm_funcs.implicit_task     = omptm_omp5__implicit_task;
        omptm_funcs.work              = NULL;
        omptm_funcs.sync_region       = NULL;
    } else if (omptm_version == OMPTM_FREE_AGENTS) {
        verbose(VB_OMPT, "Enabling experimental support with free agent threads");
        omptm_funcs.init              = omptm_free_agents__init;
        omptm_funcs.finalize          = omptm_free_agents__finalize;
        omptm_funcs.into_mpi          = omptm_free_agents__IntoBlockingCall;
        omptm_funcs.outof_mpi         = omptm_free_agents__OutOfBlockingCall;
        omptm_funcs.lend_from_api     = NULL;
        omptm_funcs.thread_begin      = omptm_free_agents__thread_begin;
        omptm_funcs.thread_end        = NULL;
        omptm_funcs.thread_role_shift = NULL;
        omptm_funcs.parallel_begin    = omptm_free_agents__parallel_begin;
        omptm_funcs.parallel_end      = omptm_free_agents__parallel_end;
        omptm_funcs.task_create       = omptm_free_agents__task_create;
        omptm_funcs.task_schedule     = omptm_free_agents__task_schedule;
        omptm_funcs.implicit_task     = omptm_free_agents__implicit_task;
        omptm_funcs.work              = NULL;
        omptm_funcs.sync_region       = omptm_free_agents__sync_region;
    }
    else if (omptm_version == OMPTM_ROLE_SHIFT) {
        verbose(VB_OMPT, "Enabling experimental support with role shift threads");
        omptm_funcs.init              = omptm_role_shift__init;
        omptm_funcs.finalize          = omptm_role_shift__finalize;
        omptm_funcs.into_mpi          = omptm_role_shift__IntoBlockingCall;
        omptm_funcs.outof_mpi         = omptm_role_shift__OutOfBlockingCall;
        omptm_funcs.lend_from_api     = NULL;
        omptm_funcs.thread_begin      = omptm_role_shift__thread_begin;
        omptm_funcs.thread_end        = NULL;
        omptm_funcs.thread_role_shift = omptm_role_shift__thread_role_shift;
        omptm_funcs.parallel_begin    = omptm_role_shift__parallel_begin;
        omptm_funcs.parallel_end      = omptm_role_shift__parallel_end;
        omptm_funcs.task_create       = omptm_role_shift__task_create;
        omptm_funcs.task_schedule     = omptm_role_shift__task_schedule;
        omptm_funcs.implicit_task     = NULL;
        omptm_funcs.work              = NULL;
        omptm_funcs.sync_region       = NULL;
    }
}

void omptool__into_blocking_call(void) {
    if (omptm_funcs.into_mpi) {
        omptm_funcs.into_mpi();
    }
}

void omptool__outof_blocking_call(void) {
    if (omptm_funcs.outof_mpi) {
        omptm_funcs.outof_mpi();
    }
}

void omptool__lend_from_api(void) {
    if (omptm_funcs.lend_from_api) {
        omptm_funcs.lend_from_api();
    }
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static bool dlb_initialized_through_ompt = false;
static const char *openmp_runtime_version;
static ompt_set_callback_t set_callback_fn = NULL;

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
    /* when GCC 11 is released: else if "gomp", print GOMP_SPINCOUNT */

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

        setup_omp_fn_ptrs(options.omptm_version);

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
            * remember if succeded to finalize it when ompt_finalize is invoked. */
            err = DLB_Init(0, NULL, NULL);
            if (err == DLB_SUCCESS) {
                dlb_initialized_through_ompt = true;
            } else {
                verbose(VB_OMPT, "DLB_Init: %s", DLB_Strerror(err));
            }
        }

        verbose(VB_OMPT, "Initializing OMPT module");

        /* Register OMPT callbacks */
        set_callback_fn = (ompt_set_callback_t)lookup("ompt_set_callback");
        if (set_callback_fn) {
            err = 0;
            if (omptm_funcs.thread_begin) {
                err += set_ompt_callback(
                        ompt_callback_thread_begin,
                        (ompt_callback_t)omptm_funcs.thread_begin);
            }
            if (omptm_funcs.thread_end) {
                err += set_ompt_callback(
                        ompt_callback_thread_end,
                        (ompt_callback_t)omptm_funcs.thread_end);
            }
            if (omptm_funcs.thread_role_shift) {
                err += set_ompt_callback(
                        ompt_callback_thread_role_shift,
                        (ompt_callback_t)omptm_funcs.thread_role_shift);
            }
            if (omptm_funcs.parallel_begin) {
                err += set_ompt_callback(
                        ompt_callback_parallel_begin,
                        (ompt_callback_t)omptm_funcs.parallel_begin);
            }
            if (omptm_funcs.parallel_end) {
                err += set_ompt_callback(
                        ompt_callback_parallel_end,
                        (ompt_callback_t)omptm_funcs.parallel_end);
            }
            if (omptm_funcs.task_create) {
                err += set_ompt_callback(
                        ompt_callback_task_create,
                        (ompt_callback_t)omptm_funcs.task_create);
            }
            if (omptm_funcs.task_schedule) {
                err += set_ompt_callback(
                        ompt_callback_task_schedule,
                        (ompt_callback_t)omptm_funcs.task_schedule);
            }
            if (omptm_funcs.implicit_task) {
                err += set_ompt_callback(
                        ompt_callback_implicit_task,
                        (ompt_callback_t)omptm_funcs.implicit_task);
            }
            if (omptm_funcs.work) {
                err += set_ompt_callback(
                        ompt_callback_work,
                        (ompt_callback_t)omptm_funcs.work);
            }
            if (omptm_funcs.sync_region) {
                err += set_ompt_callback(
                        ompt_callback_sync_region,
                        (ompt_callback_t)omptm_funcs.sync_region);
            }
        } else {
            err = 1;
            verbose(VB_OMPT, "Could not look up function \"ompt_set_callback\"");
        }

        /* If callbacks are successfully registered, initialize thread manager
         * and return a non-zero value to activate the tool */
        if (!err) {
            verbose(VB_OMPT, "OMPT callbacks succesfully registered");
            omptm_funcs.init(thread_spd->id, &options);
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
    if (set_callback_fn) {
        if (omptm_funcs.thread_begin) {
            set_callback_fn(ompt_callback_thread_begin, NULL);
        }
        if (omptm_funcs.thread_end) {
            set_callback_fn(ompt_callback_thread_end, NULL);
        }
        if (omptm_funcs.thread_role_shift) {
            set_callback_fn(ompt_callback_thread_role_shift, NULL);
        }
        if (omptm_funcs.parallel_begin) {
            set_callback_fn(ompt_callback_parallel_begin, NULL);
        }
        if (omptm_funcs.parallel_end) {
            set_callback_fn(ompt_callback_parallel_end, NULL);
        }
        if (omptm_funcs.task_create) {
            set_callback_fn(ompt_callback_task_create, NULL);
        }
        if (omptm_funcs.task_schedule) {
            set_callback_fn(ompt_callback_task_schedule, NULL);
        }
        if (omptm_funcs.implicit_task) {
            set_callback_fn(ompt_callback_implicit_task, NULL);
        }
        if (omptm_funcs.work) {
            set_callback_fn(ompt_callback_work, NULL);
        }
        if (omptm_funcs.sync_region) {
            set_callback_fn(ompt_callback_sync_region, NULL);
        }
    }

    /* Finalize thread manager */
    if (omptm_funcs.finalize) {
        omptm_funcs.finalize();
    }

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
