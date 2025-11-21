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
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/dlb_common.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "talp/talp_openmp.h"

#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/syscall.h>


static omptool_callback_funcs_t omptool_funcs = {0};
static omptool_event_funcs_t talp_funcs = {0};
static omptool_event_funcs_t omptm_funcs = {0};
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
/*  OpenMP callbacks                                                             */
/*********************************************************************************/

static void omptool_callback__thread_begin(
        ompt_thread_t thread_type,
        ompt_data_t *thread_data) {

    spd_enter_dlb(thread_spd);

    verbose(VB_OMPT, "native-thread-begin event");

    if (talp_funcs.thread_begin) {
        talp_funcs.thread_begin(thread_type);
    }
    if (omptm_funcs.thread_begin) {
        omptm_funcs.thread_begin(thread_type);
    }
}

static void omptool_callback__thread_end(
        ompt_data_t *thread_data) {

    verbose(VB_OMPT, "native-thread-end event");

    if (talp_funcs.thread_end) {
        talp_funcs.thread_end();
    }
    if (omptm_funcs.thread_end) {
        omptm_funcs.thread_end();
    }
}


/* Parallel data storage from level 1 will be reused, other levels will be
 * allocated on demand */
static omptool_parallel_data_t omptool_parallel_data_level1 = {.level = 1};

static void omptool_callback__parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_parallelism,
        int flags,
        const void *codeptr_ra) {
    if (flags & ompt_parallel_team) {
        /* Obtain the nesting level of the generating parallel region and save
         * some info in parallel_data */
        omptool_parallel_data_t *omptool_parallel_data = NULL;
        if (encountering_task_frame->exit_frame.ptr == NULL) {
            /* No exit frame means inital task, so this parallel is level 1 */
            omptool_parallel_data = &omptool_parallel_data_level1;
        } else if (encountering_task_data->value > 0) {
            /* Allocate new data */
            omptool_parallel_data = malloc(sizeof(omptool_parallel_data_t));
            omptool_parallel_data->level = encountering_task_data->value + 1;
        }

        ensure(omptool_parallel_data != NULL, "Unhandled case in %s",__func__);

        omptool_parallel_data->codeptr_ra = codeptr_ra;
        omptool_parallel_data->requested_parallelism = requested_parallelism;
        parallel_data->ptr = omptool_parallel_data;

        if (omptool_parallel_data->level == 1) {
            verbose(VB_OMPT, "parallel-begin event");
        } else {
            verbose(VB_OMPT, "parallel-begin event: nesting level %u",
                    omptool_parallel_data->level);
        }

        /* Finally, invoke TALP or OMPTM if needed */
        if (talp_funcs.parallel_begin) {
            talp_funcs.parallel_begin(omptool_parallel_data);
        }
        if (omptm_funcs.parallel_begin) {
            omptm_funcs.parallel_begin(omptool_parallel_data);
        }
    } else {
        /* ompt_parallel_league ? not supported */
    }
}

static void omptool_callback__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra) {
    if (flags & ompt_parallel_team) {
        omptool_parallel_data_t *omptool_parallel_data = parallel_data->ptr;

        if (omptool_parallel_data->level == 1) {
            verbose(VB_OMPT, "parallel-end event");
        } else {
            verbose(VB_OMPT, "parallel-end event: nesting level %u",
                    omptool_parallel_data->level);
        }

        if (talp_funcs.parallel_end) {
            talp_funcs.parallel_end(omptool_parallel_data);
        }
        if (omptm_funcs.parallel_end) {
            omptm_funcs.parallel_end(omptool_parallel_data);
        }

        /* Deallocate if needed */
        if (omptool_parallel_data->level > 1) {
            free(parallel_data->ptr);
        }

        instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
    }
}

static void omptool_callback__task_create(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *new_task_data,
        int flags,
        int has_dependences,
        const void *codeptr_ra) {
    if (flags & ompt_task_explicit) {
        /* Pass nesting level */
        new_task_data->value = encountering_task_data->value;

        verbose(VB_OMPT, "task-create event");

        if (talp_funcs.task_create) {
            talp_funcs.task_create();
        }
        if (omptm_funcs.task_create) {
            omptm_funcs.task_create();
        }
    }
}

static void omptool_callback__task_schedule(
        ompt_data_t *prior_task_data,
        ompt_task_status_t prior_task_status,
        ompt_data_t *next_task_data) {
    if (prior_task_status == ompt_task_complete) {

        verbose(VB_OMPT, "task-schedule event: task complete");

        if (talp_funcs.task_complete) {
            talp_funcs.task_complete();
        }
        if (omptm_funcs.task_complete) {
            omptm_funcs.task_complete();
        }

        instrument_event(BINDINGS_EVENT, 0, EVENT_END);

    } else if (prior_task_status == ompt_task_switch) {

        verbose(VB_OMPT, "task-schedule event: task switch");

        if (talp_funcs.task_switch) {
            talp_funcs.task_switch();
        }
        if (omptm_funcs.task_switch) {
            omptm_funcs.task_switch();
        }

        instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
    }
}

static void omptool_callback__implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int actual_parallelism,
        unsigned int index,
        int flags) {

    if (flags & ompt_task_implicit) {
        if (endpoint == ompt_scope_begin
                && parallel_data != NULL
                && parallel_data->ptr != NULL) {

            omptool_parallel_data_t *omptool_parallel_data = parallel_data->ptr;
            if (index == 0) {
                omptool_parallel_data->actual_parallelism = actual_parallelism;
            }

            /* Pass nesting level to implicit task */
            task_data->value = omptool_parallel_data->level;

            verbose(VB_OMPT, "implicit-task-begin event: into parallel function");

            if (talp_funcs.into_parallel_function) {
                talp_funcs.into_parallel_function(omptool_parallel_data, index);
            }
            if (omptm_funcs.into_parallel_function) {
                omptm_funcs.into_parallel_function(omptool_parallel_data, index);
            }

            instrument_event(BINDINGS_EVENT, sched_getcpu()+1, EVENT_BEGIN);
        }
        else if (endpoint == ompt_scope_end) {
            /* Note: parallel_data is NULL */

            verbose(VB_OMPT, "implicit-task-end event");

            if (index == 0) {
                /* The primary thread state is already controlled in the parallel-end event */
            } else {
                /* This event is emitted differently across OpenMP implementations.
                 *
                 * In LLVM (tested with earlier versions until 19), team-worker threads are
                 * typically suspended during the implicit barrier at the end of a parallel
                 * region, and this event is not triggered until they resume at the start of
                 * the next parallel region.
                 *
                 * Cray PE (testing with versions 8.3 - 8.5, based on LLVM 17) invokes this
                 * event for each team-worker thread, but sometimes after the primary thread
                 * has already triggered the parallel-end event. This can introduce race
                 * conditions that cause the timing metrics to be attributed to different states,
                 * potentially leading to inconsistent efficiency results.
                 */
                if (talp_funcs.outof_parallel_function) {
                    talp_funcs.outof_parallel_function();
                }
                if (omptm_funcs.outof_parallel_function) {
                    omptm_funcs.outof_parallel_function();
                }

                instrument_event(BINDINGS_EVENT, 0, EVENT_END);
            }
        }
    }
    else if (flags & ompt_task_initial) {
        if (endpoint == ompt_scope_begin) {
            verbose(VB_OMPT, "initial-task-begin event");
        } else if (endpoint == ompt_scope_end) {
            verbose(VB_OMPT, "initial-task-end event");
        }
    }
}


static inline void into_parallel_implicit_barrier(omptool_parallel_data_t *data) {
    if (talp_funcs.into_parallel_implicit_barrier) {
        talp_funcs.into_parallel_implicit_barrier(data);
    }
    if (omptm_funcs.into_parallel_implicit_barrier) {
        omptm_funcs.into_parallel_implicit_barrier(data);
    }

    instrument_event(BINDINGS_EVENT, 0, EVENT_END);
}

static inline void into_parallel_sync(omptool_parallel_data_t *data) {
    if (talp_funcs.into_parallel_sync) {
        talp_funcs.into_parallel_sync(data);
    }
    if (omptm_funcs.into_parallel_sync) {
        omptm_funcs.into_parallel_sync(data);
    }
}

static inline void outof_parallel_sync(omptool_parallel_data_t *data) {
    if (talp_funcs.outof_parallel_sync) {
        talp_funcs.outof_parallel_sync(data);
    }
    if (omptm_funcs.outof_parallel_sync) {
        omptm_funcs.outof_parallel_sync(data);
    }
}

/* Warning: Newer LLVM versions already use the new
 * `ompt_sync_region_barrier_implicit_parallel` enum, but older versions and
 * Cray runtimes still use the deprecated `ompt_sync_region_barrier_implicit`.
 * This enum value is also used for implicit barriers in a **single** region,
 * but we can still identify the implicit barrier of a parallel region
 * comparing the codeptr_ra, which will be NULL for all team-worker threads,
 * and equal to the codeptr_ra from parallel_begin for the primary thread.
 */
static void omptool_callback__sync_region(
        ompt_sync_region_t kind,
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        const void *codeptr_ra) {
    if (parallel_data != NULL && parallel_data->ptr != NULL) {
        omptool_parallel_data_t *data = parallel_data->ptr;
        switch (kind) {
            case ompt_sync_region_barrier_implicit:
                /* deprecated enum, includes implicit barriers from parallel,
                 * single, workshare, etc. */
                if (codeptr_ra == NULL || codeptr_ra == data->codeptr_ra) {

                    /* Note: Cray OpenMP still uses this enum, but apparently
                     * only for parallel region with no tasks */

                    /* implicit barrier in parallel region only if codeptr_ra is NULL
                     * or equal to parallel region's */

                    if (endpoint == ompt_scope_begin) {
                        verbose(VB_OMPT, "implicit-barrier-begin event: "
                                "end of parallel function (deprecated enum)");
                        into_parallel_implicit_barrier(data);
                    }
                    else if (endpoint == ompt_scope_end) {
                        verbose(VB_OMPT, "implicit-barrier-end event: "
                                "end of parallel function (deprecated enum)");
                    }

                } else {

                    /* other implicit barriers */

                    if (endpoint == ompt_scope_begin) {
                        verbose(VB_OMPT, "implicit-barrier-begin event: sync region");
                        into_parallel_sync(data);
                    } else if (endpoint == ompt_scope_end) {
                        verbose(VB_OMPT, "implicit-barrier-end event: sync region");
                        outof_parallel_sync(data);
                    }
                }
                break;

            case ompt_sync_region_barrier_explicit:
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "explicit-barrier-begin event");
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "explicit-barrier-end event");
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_barrier_implementation:
                verbose(VB_OMPT, "Implementation-Specific Barrier event");
                if (endpoint == ompt_scope_begin) {
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_taskwait:
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "taskwait-begin event");
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "taskwait-end event");
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_taskgroup:
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "taskgroup-begin event");
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "taskgroup-end event");
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_barrier_implicit_workshare:
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "implicit-barrier-begin event: workshare");
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "implicit-barrier-end event: workshare");
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_barrier_teams:
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "implicit-barrier-begin event: teams");
                    into_parallel_sync(data);
                } else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "implicit-barrier-end event: teams");
                    outof_parallel_sync(data);
                }
                break;

            case ompt_sync_region_reduction:
                /* For now we don't do anything with reductions */
                break;

            case ompt_sync_region_barrier_implicit_parallel:
                /* new enum in OMP 5.1 */
                if (endpoint == ompt_scope_begin) {
                    verbose(VB_OMPT, "implicit-barrier-begin event: end of parallel function");
                    into_parallel_implicit_barrier(data);
                }
                else if (endpoint == ompt_scope_end) {
                    verbose(VB_OMPT, "implicit-barrier-end event: end of parallel function");
                }
                break;

            default:
                break;
        }
    }
}


/*********************************************************************************/
/*  Function pointers setup                                                      */
/*********************************************************************************/

/* For testing purposes only, override function pointers */
static const omptool_event_funcs_t *test_talp_funcs = NULL;
static const omptool_event_funcs_t *test_omptm_funcs = NULL;
void omptool_testing__setup_event_fn_ptrs(
        const omptool_event_funcs_t *talp_fns,
        const omptool_event_funcs_t *omptm_fns) {
    test_talp_funcs = talp_fns;
    test_omptm_funcs = omptm_fns;
}

static void setup_omp_fn_ptrs(omptm_version_t omptm_version, bool talp_openmp) {

    /* talp_funcs */
    if (talp_openmp) {
        verbose(VB_OMPT, "Enabling OMPT support for TALP");
        if (test_talp_funcs) {
            talp_funcs = *test_talp_funcs;
        } else {
            talp_funcs = talp_events_vtable;
        }
    } else {
        talp_funcs = (const omptool_event_funcs_t) {};
    }

    /* omptm_funcs */
    switch(omptm_version) {
        case OMPTM_NONE:
            omptm_funcs = (const omptool_event_funcs_t) {};
            break;
        case OMPTM_OMP5:
            verbose(VB_OMPT, "Enabling OMPT support for OpenMP 5.0");
            if (test_omptm_funcs) {
                omptm_funcs = *test_omptm_funcs;
            } else {
                omptm_funcs = omptm_omp5_events_vtable;
            }
            break;
        case OMPTM_FREE_AGENTS:
            verbose(VB_OMPT, "Enabling experimental support with free agent threads");
            if (test_omptm_funcs) {
                omptm_funcs = *test_omptm_funcs;
            } else {
                omptm_funcs = omptm_free_agents_events_vtable;
            }
            break;
        case OMPTM_ROLE_SHIFT:
            verbose(VB_OMPT, "Enabling experimental support with role shift threads");
            if (test_omptm_funcs) {
                omptm_funcs = *test_omptm_funcs;
            } else {
                omptm_funcs = omptm_role_shift_events_vtable;
            }
            break;
    }

    /* The following callbacks are centralized and always registered, even if both
     * omptm and talp are disabled. It is deciced this way to allow event tracing and
     * debugging events entry via verbose messages. */
    omptool_funcs = (const omptool_callback_funcs_t) {
        .thread_begin      = omptool_callback__thread_begin,
        .thread_end        = omptool_callback__thread_end,
        .parallel_begin    = omptool_callback__parallel_begin,
        .parallel_end      = omptool_callback__parallel_end,
        .task_create       = omptool_callback__task_create,
        .task_schedule     = omptool_callback__task_schedule,
        .implicit_task     = omptool_callback__implicit_task,
        .sync_region       = omptool_callback__sync_region,
    };

    /* The following function is a custom callback and it's only used in the
     * experimental role-shift thread manager */
    if (omptm_version == OMPTM_ROLE_SHIFT) {
        omptool_funcs.thread_role_shift = omptm_role_shift__thread_role_shift;
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

    int error = 0;
    set_callback_fn = (ompt_set_callback_t)lookup("ompt_set_callback");
    if (set_callback_fn) {
            if (omptool_funcs.thread_begin) {
                error += set_ompt_callback(
                        ompt_callback_thread_begin,
                        (ompt_callback_t)omptool_funcs.thread_begin);
            }
            if (omptool_funcs.thread_end) {
                error += set_ompt_callback(
                        ompt_callback_thread_end,
                        (ompt_callback_t)omptool_funcs.thread_end);
            }
            if (omptool_funcs.thread_role_shift) {
                error += set_ompt_callback(
                        ompt_callback_thread_role_shift,
                        (ompt_callback_t)omptool_funcs.thread_role_shift);
            }
            if (omptool_funcs.parallel_begin) {
                error += set_ompt_callback(
                        ompt_callback_parallel_begin,
                        (ompt_callback_t)omptool_funcs.parallel_begin);
            }
            if (omptool_funcs.parallel_end) {
                error += set_ompt_callback(
                        ompt_callback_parallel_end,
                        (ompt_callback_t)omptool_funcs.parallel_end);
            }
            if (omptool_funcs.task_create) {
                error += set_ompt_callback(
                        ompt_callback_task_create,
                        (ompt_callback_t)omptool_funcs.task_create);
            }
            if (omptool_funcs.task_schedule) {
                error += set_ompt_callback(
                        ompt_callback_task_schedule,
                        (ompt_callback_t)omptool_funcs.task_schedule);
            }
            if (omptool_funcs.implicit_task) {
                error += set_ompt_callback(
                        ompt_callback_implicit_task,
                        (ompt_callback_t)omptool_funcs.implicit_task);
            }
            if (omptool_funcs.sync_region) {
                error += set_ompt_callback(
                        ompt_callback_sync_region,
                        (ompt_callback_t)omptool_funcs.sync_region);
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
        if (omptool_funcs.thread_begin) {
            set_callback_fn(ompt_callback_thread_begin, NULL);
        }
        if (omptool_funcs.thread_end) {
            set_callback_fn(ompt_callback_thread_end, NULL);
        }
        if (omptool_funcs.thread_role_shift) {
            set_callback_fn(ompt_callback_thread_role_shift, NULL);
        }
        if (omptool_funcs.parallel_begin) {
            set_callback_fn(ompt_callback_parallel_begin, NULL);
        }
        if (omptool_funcs.parallel_end) {
            set_callback_fn(ompt_callback_parallel_end, NULL);
        }
        if (omptool_funcs.task_create) {
            set_callback_fn(ompt_callback_task_create, NULL);
        }
        if (omptool_funcs.task_schedule) {
            set_callback_fn(ompt_callback_task_schedule, NULL);
        }
        if (omptool_funcs.implicit_task) {
            set_callback_fn(ompt_callback_implicit_task, NULL);
        }
        if (omptool_funcs.sync_region) {
            set_callback_fn(ompt_callback_sync_region, NULL);
        }
    }
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static bool dlb_initialized_through_ompt = false;
static bool omptool_initialized = false;
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

        /* Disable --ompt-thread-manager if no LeWI or DROM */
        if (!(options.lewi || options.drom)) {
            options.omptm_version = OMPTM_NONE;
        }

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
            /* we save the thread ID that initialized OMPT */
            tool_data->value = syscall(SYS_gettid);
            omptool_initialized = true;
            return 1;
        }

        /* Otherwise, finalize DLB if init succeeded */
        if (dlb_initialized_through_ompt) {
            DLB_Finalize();
            dlb_initialized_through_ompt = false;
        }

        warning("DLB could not register itself as OpenMP tool");
    }

    options_finalize(&options);

    return 0;
}

static void omptool_finalize(ompt_data_t *tool_data) {

    /* Protect finalization against forks and other double calls */
    if (tool_data->value == (uint64_t)syscall(SYS_gettid)
            && omptool_initialized) {

        verbose(VB_OMPT, "Finalizing OMPT module");

        /* Finalize DLB if needed */
        if (dlb_initialized_through_ompt) {
            DLB_Finalize();
        }

        /* Disable OMPT callbacks */
        unset_ompt_callbacks();

        /* Finalize modules */
        omptool__finalize();

        omptool_initialized = false;
        dlb_initialized_through_ompt = false;
    }
}


#ifndef OMPT_MULTIPLEX_H
DLB_EXPORT_SYMBOL
#endif
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    openmp_runtime_version = runtime_version;
    static ompt_start_tool_result_t tool = {
        .initialize = omptool_initialize,
        .finalize   = omptool_finalize,
        .tool_data  = {0}
    };
    return &tool;
}
