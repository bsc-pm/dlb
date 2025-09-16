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

#ifndef GPU_RECORD_UTILS_H
#define GPU_RECORD_UTILS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t start;
    uint64_t end;
} gpu_record_t;

int gpu_record_clean_and_merge(gpu_record_t *records, int num_records);
uint64_t gpu_record_get_duration(const gpu_record_t *records, int num_records);
uint64_t gpu_record_get_memory_exclusive_duration(
        const gpu_record_t *mem_records, int mem_count,
        const gpu_record_t *kernel_records, int kernel_count);

#endif /*GPU_RECORD_UTILS_H  */
