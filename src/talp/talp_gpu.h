/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#include <stddef.h>
#include <stdint.h>

typedef struct SubProcessDescriptor subprocess_descriptor_t;
typedef struct gpu_timers_t gpu_timers_t;
typedef struct gpu_device_entry_t gpu_device_entry_t;

int  talp_gpu_init(const subprocess_descriptor_t *spd);
void talp_gpu_finalize(void);
uint64_t talp_gpu_local_to_unique_id(uint32_t local_id);
void talp_gpu_enter_runtime(void);
void talp_gpu_exit_runtime(void);
void talp_gpu_collect(gpu_timers_t *out, size_t capacity, uint64_t *out_mask);

#endif /* TALP_GPU_H */
