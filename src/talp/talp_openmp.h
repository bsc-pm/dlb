/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef TALP_OPENMP_H
#define TALP_OPENMP_H

#include "LB_numThreads/omp-tools.h"
#include <sys/types.h>

typedef struct Options options_t;
typedef struct omptool_parallel_data_t omptool_parallel_data_t;

/* TALP OpenMP functions */
void talp_openmp_init(pid_t, const options_t*);
void talp_openmp_finalize(void);
void talp_openmp_thread_begin(ompt_thread_t thread_type);
void talp_openmp_thread_end(void);
void talp_openmp_parallel_begin(omptool_parallel_data_t *parallel_data);
void talp_openmp_parallel_end(omptool_parallel_data_t *parallel_data);
void talp_openmp_into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index);
void talp_openmp_outof_parallel_function(void);
void talp_openmp_into_parallel_implicit_barrier(
        omptool_parallel_data_t *parallel_data);
void talp_openmp_into_parallel_sync(omptool_parallel_data_t *parallel_data);
void talp_openmp_outof_parallel_sync(omptool_parallel_data_t *parallel_data);
void talp_openmp_task_create(void);
void talp_openmp_task_complete(void);
void talp_openmp_task_switch(void);

#endif /* TALP_OPENMP_H */
