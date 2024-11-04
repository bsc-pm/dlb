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

#ifndef OMPTOOL_H
#define OMPTOOL_H

#include "LB_numThreads/omp-tools.h"
#include "support/atomic.h"
#include "support/options.h"

/* Non-device OMPT callbacks in OpenMP 5.2 (unused callbacks are commented) */
typedef struct omptool_callback_funcs_t {
    ompt_callback_thread_begin_t        thread_begin;
    ompt_callback_thread_end_t          thread_end;
    ompt_callback_thread_role_shift_t   thread_role_shift;
    ompt_callback_parallel_begin_t      parallel_begin;
    ompt_callback_parallel_end_t        parallel_end;
    /* ompt_callback_work_t                work; */
    /* ompt_callback_dispatch_t            dispatch; */
    ompt_callback_task_create_t         task_create;
    /* ompt_callback_dependences_t         dependences; */
    /* ompt_callback_task_dependence_t     task_dependence; */
    ompt_callback_task_schedule_t       task_schedule;
    ompt_callback_implicit_task_t       implicit_task;
    /* ompt_callback_masked_t              masked; */
    ompt_callback_sync_region_t         sync_region;
    /* ompt_callback_mutex_acquire_t       mutex_acquire; */
    /* ompt_callback_mutex_t               mutex; */
    /* ompt_callback_nest_lock_t           nest_lock_t; */
    /* ompt_callback_flush_t               flush; */
    /* ompt_callback_cancel_t              cancel; */
} omptool_callback_funcs_t;

typedef struct DLB_ALIGN_CACHE omptool_parallel_data_t {
    bool         ready;
    unsigned int level;
    const void  *codeptr_ra;
    void        *omptm_parallel_data;
    void        *talp_parallel_data;

    /* The following fields are only useful for the primary thread.
     * Avoid false sharing by placing them in another cache line */
    unsigned int DLB_ALIGN_CACHE requested_parallelism;
    unsigned int actual_parallelism;
} omptool_parallel_data_t;

/* Events that may occur in a hybrid MPI-OpenMP program.
 * Some of the OpenMP events that match an OMPT callback receive less
 * arguments, or values that represent more high-level information. */
typedef struct omptool_event_funcs_t {
    /* Init & Finalize function */
    void (*init)(pid_t, const options_t*);
    void (*finalize)(void);
    /* MPI calls */
    void (*into_mpi)(void);
    void (*outof_mpi)(void);
    /* Intercept Lend */
    void (*lend_from_api)(void);
    /* Thread events */
    void (*thread_begin)(ompt_thread_t);
    void (*thread_end)(void);
    void (*thread_role_shift)(ompt_data_t*, ompt_role_t, ompt_role_t);
    /* Parallel events */
    void (*parallel_begin)(omptool_parallel_data_t*);
    void (*parallel_end)(omptool_parallel_data_t*);
    void (*into_parallel_function)(omptool_parallel_data_t*, unsigned);
    void (*into_parallel_implicit_barrier)(omptool_parallel_data_t*);
    void (*into_parallel_sync)(omptool_parallel_data_t*);
    void (*outof_parallel_sync)(omptool_parallel_data_t*);
    /* Tasking events */
    void (*task_create)(void);
    void (*task_complete)(void);
    void (*task_switch)(void);
} omptool_event_funcs_t;

void omptool__into_blocking_call(void);
void omptool__outof_blocking_call(void);
void omptool__lend_from_api(void);
void omptool_testing__setup_event_fn_ptrs(
        const omptool_event_funcs_t *talp_test_funcs,
        const omptool_event_funcs_t *omptm_test_funcs);

#endif /* OMPTOOL_H */
