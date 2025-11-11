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

#ifndef OMPTM_ROLE_SHIFT_H
#define OMPTM_ROLE_SHIFT_H

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptool.h"
#include "support/options.h"

typedef struct omptool_parallel_data_t omptool_parallel_data_t;

void omptm_role_shift__init(pid_t process_id, const options_t *options);
void omptm_role_shift__finalize(void);
void omptm_role_shift__IntoBlockingCall(void);
void omptm_role_shift__OutOfBlockingCall(void);

void omptm_role_shift__thread_begin(ompt_thread_t thread_type);

void omptm_role_shift__thread_role_shift(
                ompt_data_t *thread_data,
                ompt_role_t prior_role,
                ompt_role_t next_role);

void omptm_role_shift__parallel_begin(omptool_parallel_data_t *parallel_data);
void omptm_role_shift__parallel_end(omptool_parallel_data_t *parallel_data);
void omptm_role_shift__task_create(void);
void omptm_role_shift__task_complete(void);
void omptm_role_shift__task_switch(void);

extern const omptool_event_funcs_t omptm_role_shift_events_vtable;

/* Functions for testing purposes */
int  omptm_role_shift_testing__get_num_free_agents(void);
int  omptm_role_shift_testing__get_num_registered_threads(void);
int  omptm_role_shift_testing__get_current_parallel_size(void);
void omptm_role_shift_testing__set_pending_tasks(unsigned int num_tasks);
unsigned int omptm_role_shift_testing__get_pending_tasks(void);
void omptm_role_shift_testing__set_global_tid(int tid);
bool omptm_role_shift_testing__in_parallel(void);
int omptm_role_shift_testing__get_id_from_cpu(int cpuid);

typedef struct CPU_Data cpu_data_t;
cpu_data_t* omptm_role_shift_testing__get_cpu_data_ptr(void);
int* omptm_role_shift_testing__get_cpu_by_id_ptr(void);

#endif /* OMPTM_ROLE_SHIFT_H */
