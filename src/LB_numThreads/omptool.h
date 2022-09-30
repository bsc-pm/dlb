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

#ifndef OMPTOOL_H
#define OMPTOOL_H

#include "LB_numThreads/omp-tools.h"
#include "support/options.h"

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
} funcs_t;

void omptool__into_blocking_call(void);
void omptool__outof_blocking_call(void);
void omptool__lend_from_api(void);
void omptool_testing__setup_omp_fn_ptrs(const funcs_t *talp_tests_funcs,
        const funcs_t *omptm_test_funcs);

#endif /* OMPTOOL_H */
