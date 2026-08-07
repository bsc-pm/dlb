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

#include "rocprofiler/v2/rocprofiler.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/* Host Runtime callback */
static rocprofiler_sync_callback_t       cb_runtime_fn = NULL;

/* Async callback */
static rocprofiler_buffer_callback_t     cb_buffer_fn = NULL;

enum {
    SESSION_IS_VALID  = 1 << 0,
    SESSION_IS_ACTIVE = 1 << 1,
};

/* Internal functions for buffer management */

typedef union rocprofiler_record_t {
    rocprofiler_record_tracer_t   record_tracer;
    rocprofiler_record_profiler_t record_profiler;
} rocprofiler_record_t;

typedef struct event_buffer_t {
    rocprofiler_record_t *records;
    size_t  capacity;
    size_t  num_records;
} event_buffer_t;

static event_buffer_t buffer = {0};

static void create_buffer(size_t size) {

    if (buffer.records != NULL) {
        fprintf(stderr, "rocprofilerv2_stub: buffer not empty\n");
        abort();
    }

    buffer = (event_buffer_t){
        .records = malloc(size),
        .capacity = size / sizeof(rocprofiler_record_t),
        .num_records = 0,
    };
}

static void flush_buffer(void) {

    rocprofiler_session_id_t session_id = {0};
    rocprofiler_buffer_id_t  buffer_id = {0};

    const rocprofiler_record_t *begin = buffer.records;
    const rocprofiler_record_t *end = buffer.records + buffer.num_records;

    cb_buffer_fn(
            (rocprofiler_record_header_t*)begin,
            (rocprofiler_record_header_t*)end,
            session_id, buffer_id);

    free(buffer.records);
    buffer = (event_buffer_t){0};
}

static void add_record(rocprofiler_record_t *input) {

    rocprofiler_record_t *new_record = &buffer.records[buffer.num_records];

    *new_record = *input;
    ++buffer.num_records;

    if (buffer.num_records >= buffer.capacity) {
        flush_buffer();
    }
}

static uint64_t get_time(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

/*** rocprofiler ***/

rocprofiler_status_t rocprofiler_initialize(void)
{
    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_finalize(void)
{
    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_create_session(rocprofiler_replay_mode_t replay_mode,
                           rocprofiler_session_id_t* session_id)
{
    if (session_id == NULL) return ROCPROFILER_STATUS_ERROR;
    if (session_id->handle != 0) return ROCPROFILER_STATUS_ERROR;

    session_id->handle = SESSION_IS_VALID;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_start_session(rocprofiler_session_id_t session_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) return ROCPROFILER_STATUS_ERROR;

    session_id.handle |= SESSION_IS_ACTIVE;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_terminate_session(rocprofiler_session_id_t session_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) return ROCPROFILER_STATUS_ERROR;

    session_id.handle &= ~SESSION_IS_ACTIVE;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_destroy_session(rocprofiler_session_id_t session_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) return ROCPROFILER_STATUS_ERROR;

    session_id.handle &= ~SESSION_IS_VALID;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_create_buffer(
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_callback_t buffer_callback,
        size_t buffer_size,
        rocprofiler_buffer_id_t* buffer_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    if (buffer_id == NULL) return ROCPROFILER_STATUS_ERROR;
    if (buffer_id->value != 0) return ROCPROFILER_STATUS_ERROR;

    cb_buffer_fn = buffer_callback;

    create_buffer(buffer_size);

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_create_filter(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_kind_t filter_kind,
        rocprofiler_filter_data_t data,
        uint64_t data_count,
        rocprofiler_filter_id_t* filter_id,
        rocprofiler_filter_property_t property)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_set_filter_buffer(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_id_t filter_id,
        rocprofiler_buffer_id_t buffer_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_set_api_trace_sync_callback(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_id_t filter_id,
        rocprofiler_sync_callback_t callback)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    cb_runtime_fn = callback;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_flush_data(
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_id_t buffer_id)
{
    if ((session_id.handle & SESSION_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    flush_buffer();

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_next_record(
        const rocprofiler_record_header_t* record,
        const rocprofiler_record_header_t** next,
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_id_t buffer_id)
{
    rocprofiler_record_t *record_ = (rocprofiler_record_t*)record;
    rocprofiler_record_t *next_ = ++record_;
    *next = (rocprofiler_record_header_t*)next_;

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t rocprofiler_query_tracer_operation_name(
        rocprofiler_tracer_activity_domain_t domain,
        rocprofiler_tracer_operation_id_t operation_id,
        const char** name)
{
    if (name == NULL) return ROCPROFILER_STATUS_ERROR;

    if (domain == ACTIVITY_DOMAIN_HIP_API) {
        *name = "HIP Runtime API function name";
        return ROCPROFILER_STATUS_SUCCESS;
    }

    if (domain == ACTIVITY_DOMAIN_HIP_OPS) {
        *name = "CopyDeviceToHost";
        return ROCPROFILER_STATUS_SUCCESS;
    }

    return ROCPROFILER_STATUS_ERROR;
}

const char* rocprofiler_error_str(rocprofiler_status_t status)
{
    return "rocprofiler stub";
}


// functions called from the test to trigger GPU events

void rocprof_stub_call_runtime(void) {

    if (cb_runtime_fn != NULL) {

        cb_runtime_fn((rocprofiler_record_tracer_t){
                .domain = ACTIVITY_DOMAIN_HIP_API,
                .phase = ROCPROFILER_PHASE_ENTER},
                (rocprofiler_session_id_t){0});

        cb_runtime_fn((rocprofiler_record_tracer_t){
                .domain = ACTIVITY_DOMAIN_HIP_API,
                .phase = ROCPROFILER_PHASE_EXIT},
                (rocprofiler_session_id_t){0});
    }
}

void rocprof_stub_call_kernel(void) {

    uint64_t timestamp = get_time();
    rocprofiler_record_t record = {
        .record_profiler = {
            .header = {
                .kind = ROCPROFILER_PROFILER_RECORD,
            },
            .timestamps = {
                .begin = { .value = timestamp },
                .end = { .value = timestamp + 1 },
            },
        },
    };

    add_record(&record);
}

void rocprof_stub_call_memory_op(void) {

    uint64_t timestamp = get_time();
    rocprofiler_record_t record = {
        .record_tracer = {
            .header = {
                .kind = ROCPROFILER_TRACER_RECORD,
            },
            .domain = ACTIVITY_DOMAIN_HIP_OPS,
            .timestamps = {
                .begin = { .value = timestamp },
                .end = { .value = timestamp + 1 },
            },
        }
    };

    add_record(&record);
}
