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

#include "rocprofiler-sdk/registration.h"
#include "rocprofiler-sdk/rocprofiler.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


static rocprofiler_client_id_t client_id = {0};
static rocprofiler_tool_initialize_t tool_init = NULL;
static rocprofiler_tool_finalize_t   tool_fini = NULL;

/* Host Runtime callback */
static rocprofiler_callback_tracing_cb_t cb_runtime_fn = NULL;
static void*                             cb_runtime_userdata = NULL;

/* Async callback */
static rocprofiler_buffer_tracing_cb_t   cb_buffer_fn = NULL;
static void*                             cb_buffer_userdata = NULL;

enum {
    CONTEXT_IS_VALID  = 1 << 0,
    CONTEXT_IS_ACTIVE = 1 << 1,
};

/* Internal functions for buffer management */

enum { MAX_RECORDS = 8 };

typedef struct event_buffer_t {
    rocprofiler_record_header_t *headers[MAX_RECORDS];
    rocprofiler_record_t        *records;
    size_t  size;
    size_t  watermark;
    size_t  num_records;
} event_buffer_t;

static event_buffer_t buffer = {0};

static void create_buffer(size_t size, size_t watermark) {

    if (buffer.records != NULL) {
        fprintf(stderr, "rocprofiler_sdk_stub: buffer not empty\n");
        abort();
    }

    buffer = (const event_buffer_t){
        .headers = {0},
        .records = malloc(size),
        .size = size,
        .watermark = watermark,
        .num_records = 0,
    };
}

static void flush_buffer(void) {

    rocprofiler_context_id_t context = {0};
    rocprofiler_buffer_id_t  buffer_id = {0};
    uint64_t drop_count = 0;

    cb_buffer_fn(context, buffer_id, buffer.headers, buffer.num_records,
            cb_buffer_userdata, drop_count);

    for (size_t i = 0; i < buffer.num_records; ++i) {
        free(buffer.headers[i]);
    }
    free(buffer.records);
    buffer = (const event_buffer_t){0};
}

static void add_record(rocprofiler_record_t *input, uint32_t kind) {

    rocprofiler_record_header_t *header = malloc(sizeof(rocprofiler_record_header_t));
    rocprofiler_record_t        *record = &buffer.records[buffer.num_records];

    header->kind = kind;
    header->payload = record;
    *record = *input;

    buffer.headers[buffer.num_records] = header;
    ++buffer.num_records;

    if (buffer.num_records >= buffer.watermark / sizeof(buffer.records)) {
        flush_buffer();
    }
}

static uint64_t get_time(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

/*** registration ***/

static void stub_fini_func(rocprofiler_client_id_t id);

void rocprof_stub_init_tool(void) {

    void *handle = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        abort();
    }

    rocprofiler_configure_fn configure_fn =
        (rocprofiler_configure_fn)dlsym(handle, "rocprofiler_configure");
    if (configure_fn == NULL) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        dlclose(handle);
        abort();
    }
    dlclose(handle);

    /* call configure */
    uint32_t version = 10000;
    const char *version_string = "rocprofiler-sdk stub";
    uint32_t priority = 0;
    rocprofiler_tool_configure_result_t *cfg =
        configure_fn(version, version_string, priority, &client_id);

    if (cfg == NULL) {
        fprintf(stderr, "plugin configure returned NULL\n");
        abort();
    }

    tool_init = cfg->initialize;
    tool_fini = cfg->finalize;

    /* init */
    int error = tool_init(stub_fini_func, NULL);
    if (error != 0) {
        fprintf(stderr, "plugin init returned ERROR\n");
        abort();
    }
}

/* pointer passed to tool to trigger finalization */
static void stub_fini_func(rocprofiler_client_id_t id) {
    tool_fini(NULL);
}

/* called from tests */
void rocprof_stub_fini_tool(void) {
    stub_fini_func(client_id);
}

/*** rocprofiler ***/

rocprofiler_status_t
rocprofiler_create_context(rocprofiler_context_id_t* context_id) {

    if (context_id == NULL) return ROCPROFILER_STATUS_ERROR;
    if (context_id->handle != 0) return ROCPROFILER_STATUS_ERROR;

    context_id->handle = CONTEXT_IS_VALID;

    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_start_context(rocprofiler_context_id_t context_id) {

    if ((context_id.handle & CONTEXT_IS_VALID) == 0) return ROCPROFILER_STATUS_ERROR;

    context_id.handle |= CONTEXT_IS_ACTIVE;

    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_stop_context(rocprofiler_context_id_t context_id) {

    if ((context_id.handle & CONTEXT_IS_VALID) == 0) return ROCPROFILER_STATUS_ERROR;

    context_id.handle &= ~CONTEXT_IS_ACTIVE;

    return ROCPROFILER_STATUS_SUCCESS;
}

/* Note: doc says if invalid status is nonzero, but it appears to be a typo */
rocprofiler_status_t
rocprofiler_context_is_valid(rocprofiler_context_id_t context_id, int* status) {

    if ((context_id.handle & CONTEXT_IS_VALID) == 0) {
        *status = 0;
        return ROCPROFILER_STATUS_ERROR;
    }

    *status = 1;
    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_context_is_active(rocprofiler_context_id_t context_id, int* status) {

    if ((context_id.handle & CONTEXT_IS_ACTIVE) == 0) {
        *status = 0;
        return ROCPROFILER_STATUS_ERROR;
    }

    *status = 1;
    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_configure_callback_tracing_service(rocprofiler_context_id_t               context_id,
                                               rocprofiler_callback_tracing_kind_t    kind,
                                               const rocprofiler_tracing_operation_t* operations,
                                               size_t                            operations_count,
                                               rocprofiler_callback_tracing_cb_t callback,
                                               void* callback_args) {

    if ((context_id.handle & CONTEXT_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    if (kind != ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API) {
        return ROCPROFILER_STATUS_ERROR;
    }

    cb_runtime_fn = callback;
    cb_runtime_userdata = callback_args;

    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_query_callback_tracing_kind_operation_name(rocprofiler_callback_tracing_kind_t kind,
                                                       rocprofiler_tracing_operation_t operation,
                                                       const char**                    name,
                                                       uint64_t* name_len) {

    if (name == NULL) return ROCPROFILER_STATUS_ERROR;

    if (kind != ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API) {
        *name = "rocprofiler-sdk_stub error";
        return ROCPROFILER_STATUS_ERROR;
    }

    *name = "HIP Runtime API function name";

    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_create_buffer(rocprofiler_context_id_t        context,
                          size_t                          size,
                          size_t                          watermark,
                          rocprofiler_buffer_policy_t     policy,
                          rocprofiler_buffer_tracing_cb_t callback,
                          void*                           callback_data,
                          rocprofiler_buffer_id_t*        buffer_id) {

    if ((context.handle & CONTEXT_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    cb_buffer_fn = callback;
    cb_buffer_userdata = callback_data;

    create_buffer(size, watermark);

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t
rocprofiler_configure_buffer_tracing_service(rocprofiler_context_id_t               context_id,
                                             rocprofiler_buffer_tracing_kind_t      kind,
                                             const rocprofiler_tracing_operation_t* operations,
                                             size_t                  operations_count,
                                             rocprofiler_buffer_id_t buffer_id) {

    if ((context_id.handle & CONTEXT_IS_VALID) == 0) {
        return ROCPROFILER_STATUS_ERROR;
    }

    if (kind != ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH
            && kind != ROCPROFILER_BUFFER_TRACING_MEMORY_COPY) {
        return ROCPROFILER_STATUS_ERROR;
    }

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t
rocprofiler_flush_buffer(rocprofiler_buffer_id_t buffer_id) {

    flush_buffer();

    return ROCPROFILER_STATUS_SUCCESS;
}


rocprofiler_status_t
rocprofiler_create_callback_thread(rocprofiler_callback_thread_t* cb_thread_id) {

    return ROCPROFILER_STATUS_SUCCESS;
}

rocprofiler_status_t
rocprofiler_assign_callback_thread(rocprofiler_buffer_id_t       buffer_id,
                                   rocprofiler_callback_thread_t cb_thread_id) {

    return ROCPROFILER_STATUS_SUCCESS;
}


const char* rocprofiler_get_status_string(rocprofiler_status_t status) {

    return "rocprofiler stub";
}

// functions called from the test to trigger GPU events

void rocprof_stub_call_runtime(void) {

    if (cb_runtime_fn != NULL) {

        cb_runtime_fn((const rocprofiler_callback_tracing_record_t){
                .kind = ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
                .phase = ROCPROFILER_CALLBACK_PHASE_ENTER},
                NULL, cb_runtime_userdata);

        cb_runtime_fn((const rocprofiler_callback_tracing_record_t){
                .kind = ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
                .phase = ROCPROFILER_CALLBACK_PHASE_EXIT},
                NULL, cb_runtime_userdata);
    }
}

void rocprof_stub_call_kernel(void) {

    uint64_t timestamp = get_time();
    rocprofiler_record_t record = {
        .start_timestamp = timestamp,
        .end_timestamp = timestamp + 1,
    };

    add_record(&record, ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH);
}

void rocprof_stub_call_memory_op(void) {

    uint64_t timestamp = get_time();
    rocprofiler_record_t record = {
        .start_timestamp = timestamp,
        .end_timestamp = timestamp + 1,
    };

    add_record(&record, ROCPROFILER_BUFFER_TRACING_MEMORY_COPY);
}
