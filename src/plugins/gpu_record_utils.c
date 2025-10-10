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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Compare function for qsort */
static int compare_records(const void *a, const void *b) {
    const gpu_record_t *ra = (const gpu_record_t *)a;
    const gpu_record_t *rb = (const gpu_record_t *)b;
    return (ra->start > rb->start) - (ra->start < rb->start);
}


/* Ensure space for 1 more element */
static inline void ensure_capacity(gpu_records_buffer_t* buf) {
    enum { MAX_EVENTS = 64 * 1024 * 1024 }; // 64M events = 1GB
    if (buf->size >= buf->capacity) {
        if (buf->capacity >= MAX_EVENTS) {
            fprintf(stderr, "Too many events (> %llu), aborting\n",
                    (unsigned long long)MAX_EVENTS);
            exit(1);
        }

        size_t new_cap = buf->capacity * 2;
        if (new_cap > MAX_EVENTS) new_cap = MAX_EVENTS;

        gpu_record_t* new_data = realloc(buf->data, new_cap * sizeof(gpu_record_t));
        if (!new_data) {
            fprintf(stderr, "realloc failed\n");
            exit(1);
        }
        buf->data = new_data;
        buf->capacity = new_cap;
    }
}


static inline bool is_valid_record(uint64_t start, uint64_t end) {
    return start < end;
}


void gpu_record_init_buffer(gpu_records_buffer_t *buf, size_t initial_capacity) {
    buf->data = malloc(initial_capacity * sizeof(gpu_record_t));
    if (!buf->data) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    buf->size = 0;
    buf->capacity = initial_capacity;
}


void gpu_record_free_buffer(gpu_records_buffer_t *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}


void gpu_record_clear_buffer(gpu_records_buffer_t *buf) {
    buf->size = 0;
}


/* Add one record */
void gpu_record_append_event(gpu_records_buffer_t* buf, uint64_t start, uint64_t end) {
    ensure_capacity(buf);
    if (is_valid_record(start, end)) {
        buf->data[buf->size].start = start;
        buf->data[buf->size].end   = end;
        buf->size++;
    }
}


/* Sort and merge in place (record flattening) */
void gpu_record_flatten(gpu_records_buffer_t *buf) {

    size_t num_records = buf->size;
    gpu_record_t *records = buf->data;

    if (num_records == 0) return;

    // Sort records by start time
    qsort(records, num_records, sizeof(gpu_record_t), compare_records);

    // Merge overlapping records in-place
    size_t new_i = 0;
    for (size_t i = 1; i < num_records; ++i) {
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

    // Update new size
    buf->size = new_i + 1;
}

/* Compute total duration of records. PRE: records are sorted and merged. */
uint64_t gpu_record_get_duration(const gpu_records_buffer_t *buf) {

    uint64_t total = 0;
    size_t num_records = buf->size;
    gpu_record_t *records = buf->data;

    for (size_t i = 0; i < num_records; ++i) {
        total += records[i].end - records[i].start;
    }

    return total;
}

/* Compute exclusive duration in memory records */
uint64_t gpu_record_get_memory_exclusive_duration(
        const gpu_records_buffer_t *mem_buf,
        const gpu_records_buffer_t *kernel_buf) {

    size_t m_i = 0, k_i = 0;
    uint64_t total_exclusive = 0;

    const gpu_record_t *mem_records = mem_buf->data;
    size_t mem_count = mem_buf->size;

    const gpu_record_t *kernel_records = kernel_buf->data;
    size_t kernel_count = kernel_buf->size;

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
