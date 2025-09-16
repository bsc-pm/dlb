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

#include "plugins/gpu_record_utils.h"

#include <stdlib.h>

/* Compare function for qsort */
static int compare_records(const void *a, const void *b) {
    const gpu_record_t *ra = (const gpu_record_t *)a;
    const gpu_record_t *rb = (const gpu_record_t *)b;
    return (ra->start > rb->start) - (ra->start < rb->start);
}

/* Sort and merge in place */
int gpu_record_clean_and_merge(gpu_record_t *records, int num_records) {

    if (num_records <= 0) return 0;

    // Step 1: Filter out invalid records in-place
    int valid_count = 0;
    for (int i = 0; i < num_records; ++i) {
        if (records[i].end <= records[i].start)
            continue; // skip invalid
        records[valid_count++] = records[i];
    }

    if (valid_count <= 1) return valid_count;

    // Step 2: Sort valid records by start time
    qsort(records, (size_t)valid_count, sizeof(gpu_record_t), compare_records);

    // Step 3: Merge overlapping records in-place
    int new_i = 0;
    for (int i = 1; i < valid_count; ++i) {
        if (records[new_i].end >= records[i].start) {
            // Overlapping
            if (records[i].end > records[new_i].end) {
                records[new_i].end = records[i].end;
            }
            // else: fully contained, do nothing
        } else {
            // No overlap, move forward
            ++new_i;
            records[new_i] = records[i];
        }
    }

    return new_i + 1;
}

/* Compute total duration of records. PRE: records is sorted and merged. */
uint64_t gpu_record_get_duration(const gpu_record_t *records, int num_records) {
    uint64_t total = 0;
    for (int i = 0; i < num_records; ++i) {
        total += records[i].end - records[i].start;
    }
    return total;
}

/* Compute exclusive duration in memory records */
uint64_t gpu_record_get_memory_exclusive_duration(
        const gpu_record_t *mem_records, int mem_count,
        const gpu_record_t *kernel_records, int kernel_count) {

    int m_i = 0, k_i = 0;
    uint64_t total_exclusive = 0;

    while (m_i < mem_count) {
        uint64_t mem_start = mem_records[m_i].start;
        uint64_t mem_end   = mem_records[m_i].end;
        uint64_t excl_start = mem_start;
        uint64_t mem_excl = 0;

        // Skip kernel intervals that end before memory starts
        while (k_i < kernel_count && kernel_records[k_i].end <= mem_start)
            ++k_i;

        // Find kernels that may overlap
        while (k_i < kernel_count && kernel_records[k_i].start < mem_end) {
            if (kernel_records[k_i].start > excl_start) {
                mem_excl += kernel_records[k_i].start - excl_start;
            }
            if (kernel_records[k_i].end >= mem_end) {
                excl_start = mem_end;
                break;
            } else {
                excl_start = kernel_records[k_i].end;
                ++k_i;
            }
        }

        if (excl_start < mem_end) {
            mem_excl += mem_end - excl_start;
        }

        total_exclusive += mem_excl;
        ++m_i;
    }

    return total_exclusive;
}
