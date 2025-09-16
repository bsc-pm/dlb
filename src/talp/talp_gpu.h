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

#ifndef TALP_GPU_H
#define TALP_GPU_H

#include <stdint.h>

typedef void (*trigger_update_func_t)(void);

typedef struct {
    int64_t useful_time;
    int64_t communication_time;
    int64_t inactive_time;
} talp_gpu_measurements_t;

void talp_gpu_sync_measurements(void);
void talp_gpu_init(trigger_update_func_t update_fn);
void talp_gpu_finalize(void);
void talp_gpu_into_runtime_api(void);
void talp_gpu_out_of_runtime_api(void);
void talp_gpu_update_sample(talp_gpu_measurements_t measurements);

#endif /* TALP_GPU_H */
