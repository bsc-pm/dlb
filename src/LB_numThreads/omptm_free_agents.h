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

#ifndef OMPTM_FREE_AGENTS_H
#define OMPTM_FREE_AGENTS_H

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptool.h"
#include "support/options.h"

typedef struct omptool_parallel_data_t omptool_parallel_data_t;

void omptm_free_agents__init(pid_t process_id, const options_t *options);
void omptm_free_agents__finalize(void);
void omptm_free_agents__IntoBlockingCall(void);
void omptm_free_agents__OutOfBlockingCall(void);

void omptm_free_agents__thread_begin(ompt_thread_t thread_type);
void omptm_free_agents__parallel_begin(omptool_parallel_data_t *parallel_data);
void omptm_free_agents__parallel_end(omptool_parallel_data_t *parallel_data);
void omptm_free_agents__into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index);
void omptm_free_agents__task_create(void);
void omptm_free_agents__task_complete(void);
void omptm_free_agents__task_switch(void);

extern const omptool_event_funcs_t omptm_free_agents_events_vtable;

/* Functions for testing purposes */
void omptm_free_agents_testing__set_worker_binding(int cpuid);
void omptm_free_agents_testing__set_free_agent_id(int id);
void omptm_free_agents_testing__set_pending_tasks(unsigned int num_tasks);
void omptm_free_agents_testing__acquire_one_free_agent(void);
bool omptm_free_agents_testing__in_parallel(void);
bool omptm_free_agents_testing__check_cpu_in_parallel(int cpuid);
bool omptm_free_agents_testing__check_cpu_idle(int cpuid);
bool omptm_free_agents_testing__check_cpu_free_agent_enabled(int cpuid);
int  omptm_free_agents_testing__get_num_enabled_free_agents(void);
int  omptm_free_agents_testing__get_free_agent_cpu(int thread_id);
int  omptm_free_agents_testing__get_free_agent_binding(int cpuid);
int  omptm_free_agents_testing__get_free_agent_id_by_cpuid(int cpuid);
int  omptm_free_agents_testing__get_free_agent_cpuid_by_id(int thread_id);

#endif /* OMPTM_FREE_AGENTS_H */
