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

#ifndef OMPTM_OMP5_H
#define OMPTM_OMP5_H

#include "LB_numThreads/ompt.h"
#include "support/options.h"

void omptm_omp5__init(pid_t process_id, const options_t *options);
void omptm_omp5__finalize(void);
void omptm_omp5__IntoBlockingCall(void);
void omptm_omp5__OutOfBlockingCall(void);
void omptm_omp5__lend_from_api(void);

void omptm_omp5__parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_parallelism,
        int flags,
        const void *codeptr_ra);

void omptm_omp5__parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        int flags,
        const void *codeptr_ra);

void omptm_omp5__implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int actual_parallelism,
        unsigned int index,
        int flags);

#endif /* OMPTM_OMP5_H */
