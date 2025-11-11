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

#ifndef OMPTM_OMP5_H
#define OMPTM_OMP5_H

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptool.h"
#include "support/options.h"

typedef struct omptool_parallel_data_t omptool_parallel_data_t;

void omptm_omp5__init(pid_t process_id, const options_t *options);
void omptm_omp5__finalize(void);
void omptm_omp5__IntoBlockingCall(void);
void omptm_omp5__OutOfBlockingCall(void);
void omptm_omp5__lend_from_api(void);

void omptm_omp5__parallel_begin(omptool_parallel_data_t *parallel_data);
void omptm_omp5__parallel_end(omptool_parallel_data_t *parallel_data);
void omptm_omp5__into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index);
void omptm_omp5__into_parallel_implicit_barrier(
        omptool_parallel_data_t *parallel_data);

extern const omptool_event_funcs_t omptm_omp5_events_vtable;

/* Functions for testing purposes */
typedef struct array_cpuid_t array_cpuid_t;
const cpu_set_t* omptm_omp5_testing__get_active_mask(void);
const array_cpuid_t* omptm_omp5_testing__compute_and_get_cpu_bindings(void);
void omptm_omp5_testing__set_num_threads_fn(void (*fn)(int));

#endif /* OMPTM_OMP5_H */
