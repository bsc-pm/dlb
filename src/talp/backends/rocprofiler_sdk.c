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

#include "support/dlb_common.h"
#include "talp/backend.h"
#include "talp/backends/backend_utils.h"
#include "talp/backends/gpu_record_utils.h"

#ifndef __HIP_PLATFORM_AMD__
#define __HIP_PLATFORM_AMD__
#endif
#include <hip/hip_runtime_api.h>
#include <rocprofiler-sdk/registration.h>
#include <rocprofiler-sdk/rocprofiler.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>


static const core_api_t *dlb_core_api = NULL;

/*********************************************************************************/
/*  ROCm profiling (rocprofiler-sdk)                                             */
/*********************************************************************************/


#define CHECK_ROCPROFILER(call)                                                 \
    do {                                                                        \
        rocprofiler_status_t _status = (call);                                  \
        if (_status != ROCPROFILER_STATUS_SUCCESS) {                            \
            const char *_error_string = rocprofiler_get_status_string(_status); \
            PLUGIN_ERROR(                                                       \
                    "%s:%d: Function %s failed with error (%d): %s.\n",         \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
                                                                                \
            return DLB_BACKEND_ERROR;                                           \
        }                                                                       \
  } while (0)

#define CHECK_WARN_ROCPROFILER(call)                                            \
    do {                                                                        \
        rocprofiler_status_t _status = (call);                                  \
        if (_status != ROCPROFILER_STATUS_SUCCESS) {                            \
            const char *_error_string = rocprofiler_get_status_string(_status); \
            PLUGIN_WARNING(                                                     \
                    "%s:%d: Function %s failed with error (%d): %s.\n",         \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
        }                                                                       \
  } while (0)



/* --- ROCm state -------------------------------------------------------------- */

static bool rocprofiler_sdk_plugin_initialized = false;
static bool rocprofiler_sdk_plugin_started = false;
static rocprofiler_context_id_t _ctx = {0};
static rocprofiler_buffer_id_t _buffer_id = {0};
static rocprofiler_client_id_t *client_id = NULL;;
static rocprofiler_client_finalize_t finalize_tool_fn = NULL;

/* ---Host runtime events ------------------------------------------------------ */

void HIP_API_callback(
        rocprofiler_callback_tracing_record_t record,
        rocprofiler_user_data_t*              user_data,
        void*                                 callback_data) {



    const char* operation_name = NULL;

    if (plugin_is_verbose()) {
        CHECK_WARN_ROCPROFILER(
                rocprofiler_query_callback_tracing_kind_operation_name(
                    record.kind, record.operation, &operation_name, NULL));
    }

    if (record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
        PLUGIN_PRINT(" >> %s\n", operation_name);
        dlb_core_api->gpu.enter_runtime();
    } else if (record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
        PLUGIN_PRINT(" << %s\n", operation_name);
        dlb_core_api->gpu.exit_runtime();
    }
}


/* --- Local buffers management ------------------------------------------------ */

/* Local buffers for storing kernel and memory records.
 * Both records need to be kept in memory because events from the GPU profiling
 * library may arrive out-of-order across different buffer flushes.  These
 * buffers are flattened and processed only when computing durations for the
 * sample. */
static gpu_records_buffer_t kernel_buffer = {};
static gpu_records_buffer_t memory_buffer = {};
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

/* After flushing the buffers, advance safe_timestamp so that any future records
 * with start times earlier than this are ignored. */
static uint64_t safe_timestamp = 0;


/* Called by ROCPROFILER when a buffer needs to be flushed.
 * The callback receives a contiguous range of activity records [begin, end].
 * We copy only the minimal information we need (start and end timestamps)
 * into our own buffers for later processing. */
void async_events_callback(
        rocprofiler_context_id_t      context,
        rocprofiler_buffer_id_t       buffer_id,
        rocprofiler_record_header_t** headers,
        size_t                        num_headers,
        void*                         user_data,
        uint64_t                      drop_count) {

    pthread_mutex_lock(&buffer_mutex);

    for(size_t i = 0; i < num_headers; ++i) {

        rocprofiler_record_header_t *header = headers[i];

        // TODO: all records should be of category ROCPROFILER_BUFFER_CATEGORY_TRACING
        // maybe we need to check it when obtaining counters.

        if (header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH) {

            const rocprofiler_buffer_tracing_kernel_dispatch_record_t *record =
                (const rocprofiler_buffer_tracing_kernel_dispatch_record_t*)(header->payload);

            uint64_t kernel_start = record->start_timestamp;
            uint64_t kernel_end = record->end_timestamp;

            if (kernel_start >= safe_timestamp
                    && kernel_end > safe_timestamp) {

                gpu_record_append_event(&kernel_buffer, kernel_start, kernel_end);

                PLUGIN_PRINT("KERNEL: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        kernel_start, kernel_end, kernel_end - kernel_start);
            }
        }
        else if(header->kind == ROCPROFILER_BUFFER_TRACING_MEMORY_COPY) {

            const rocprofiler_buffer_tracing_memory_copy_record_t *record =
                (const rocprofiler_buffer_tracing_memory_copy_record_t*)(header->payload);

            uint64_t memory_start = record->start_timestamp;
            uint64_t memory_end = record->end_timestamp;

            if (memory_start >= safe_timestamp
                    && memory_end > safe_timestamp) {

                gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                PLUGIN_PRINT("MEMCPY: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        memory_start, memory_end, memory_end - memory_start);
            }
        }
    }

    pthread_mutex_unlock(&buffer_mutex);
}


/* --- Plugin functions -------------------------------------------------------- */

/* Function called externally by TALP to force flushing buffers */
static void rocprofiler_sdk_backend_flush(void) {

    /* Flush buffer */
    if (rocprofiler_sdk_plugin_started) {
        /* This is a blocking call, it does not return until all activity records
        * are returned using the registered callback. */
        CHECK_WARN_ROCPROFILER(rocprofiler_flush_buffer(_buffer_id));
    }

    if (rocprofiler_sdk_plugin_initialized) {
        pthread_mutex_lock(&buffer_mutex);

        /* Update safe timestamp. All future records prior to this will be ignored */
        safe_timestamp = get_timestamp();

        /* Compute kernel records duration */
        gpu_record_flatten(&kernel_buffer);
        uint64_t kernel_time = gpu_record_get_duration(&kernel_buffer);

        /* Compute memory records duration */
        gpu_record_flatten(&memory_buffer);
        uint64_t memory_time = gpu_record_get_memory_exclusive_duration(&memory_buffer, &kernel_buffer);

        /* Clear buffers */
        gpu_record_clear_buffer(&kernel_buffer);
        gpu_record_clear_buffer(&memory_buffer);

        pthread_mutex_unlock(&buffer_mutex);

        if (kernel_time > 0 || memory_time > 0) {
            /* Pack values to send to TALP */
            gpu_measurements_t measurements = {
                .useful_time = kernel_time,
                .communication_time = memory_time,
            };

            /* Call TALP */
            dlb_core_api->gpu.submit_measurements(&measurements);
        }
    }
}

static int rocprofiler_sdk_backend_probe(void) {
    return DLB_BACKEND_SUCCESS;
}

/* rocprofiler_sdk_backend_init and rocprofiler_sdk_backend_start are called
 * during DLB_Init, which is enforced before calling the real rocprofiler_configure.
 * We can't really start the profiling yet. */
static int rocprofiler_sdk_backend_init(const core_api_t *core_api) {

    if (rocprofiler_sdk_plugin_initialized) return DLB_BACKEND_ERROR;

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION
            || dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    /* Allocate local buffers */
    enum { LOCAL_BUFFER_INITIAL_CAPACITY = 256 * 1024 }; // 256k records = 4MB
    gpu_record_init_buffer(&kernel_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);
    gpu_record_init_buffer(&memory_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);

    rocprofiler_sdk_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

static int rocprofiler_sdk_backend_start(void) {
    /* no-op, see above */
    return DLB_BACKEND_SUCCESS;
}

static int rocprofiler_sdk_backend_stop(void) {

    /* Trigger rocprofiler-sdk finalization */
    if (finalize_tool_fn && client_id) {
        finalize_tool_fn(*client_id);
    }

    return DLB_BACKEND_SUCCESS;
}

static int rocprofiler_sdk_backend_finalize(void) {

    if (rocprofiler_sdk_plugin_initialized) {

        /* Free local buffers */
        gpu_record_free_buffer(&kernel_buffer);
        gpu_record_free_buffer(&memory_buffer);

        rocprofiler_sdk_plugin_initialized = false;
    }

    return DLB_BACKEND_SUCCESS;
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void) {
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "rocprofiler-sdk",
        .capabilities = {
            .gpu = true,
            .gpu_amd = true,
        },
        .probe = rocprofiler_sdk_backend_probe,
        .init = rocprofiler_sdk_backend_init,
        .start = rocprofiler_sdk_backend_start,
        .stop = rocprofiler_sdk_backend_stop,
        .finalize = rocprofiler_sdk_backend_finalize,
        .flush = rocprofiler_sdk_backend_flush,
        .get_gpu_affinity = NULL,
    };
    return &api;
}


/* --- rocprofiler-sdk register interface -------------------------------------- */

enum { TOOL_REGISTER_SUCCESS = 0 };
enum { TOOL_REGISTER_ERROR = -1 };

/* Called by rocprofiler-sdk. Plugin should have been initialized at this point. */
static int tool_init(rocprofiler_client_finalize_t fini_func, void* tool_data) {

    if (dlb_core_api == NULL
            || !rocprofiler_sdk_plugin_initialized) {
        PLUGIN_ERROR("rocprofiler-sdk tool_init function called without"
                " DLB plugin properly initialized. GPU instrumentation cannot"
                " continue. Please report bug.\n");
        return TOOL_REGISTER_ERROR;
    }

    finalize_tool_fn = fini_func;

    /* Set initial safe timestamp. All records prior to this will be ignored */
    safe_timestamp = get_timestamp();

    CHECK_ROCPROFILER(rocprofiler_create_context(&_ctx));

    /* HIP API via synchronous callback */
    rocprofiler_callback_tracing_kind_t tracing_kind =
        plugin_gpu_use_low_level_api()
        ? ROCPROFILER_CALLBACK_TRACING_HSA_CORE_API
        : ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API; // default, high level API
    CHECK_ROCPROFILER(
            rocprofiler_configure_callback_tracing_service(
                _ctx,
                tracing_kind,
                NULL,
                0,
                HIP_API_callback,
                NULL));


    /* Kernel and Memory operations via buffers and asynchronous callback */

    enum { BUFFER_SIZE = 4 * 1024 * 1024 }; // 4MB
    enum { BUFFER_WATERMARK = BUFFER_SIZE - (BUFFER_SIZE / 8) };
    CHECK_ROCPROFILER(
            rocprofiler_create_buffer(
                _ctx,
                BUFFER_SIZE,
                BUFFER_WATERMARK,
                ROCPROFILER_BUFFER_POLICY_LOSSLESS,
                async_events_callback,
                NULL,
                &_buffer_id));

    CHECK_ROCPROFILER(
            rocprofiler_configure_buffer_tracing_service(
                _ctx,
                ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
                NULL,
                0,
                _buffer_id));

    CHECK_ROCPROFILER(
            rocprofiler_configure_buffer_tracing_service(
                _ctx,
                ROCPROFILER_BUFFER_TRACING_MEMORY_COPY,
                NULL,
                0,
                _buffer_id));

    /* Create thread for flushing buffers */
    rocprofiler_callback_thread_t callback_thread = {0};
    CHECK_ROCPROFILER(rocprofiler_create_callback_thread(&callback_thread));
    CHECK_ROCPROFILER(rocprofiler_assign_callback_thread(_buffer_id, callback_thread));

    /* context check */
    int valid_ctx = 0;
    CHECK_ROCPROFILER(rocprofiler_context_is_valid(_ctx, &valid_ctx));
    if (valid_ctx == 0) {
        PLUGIN_ERROR("rocprofiler-sdk tool_init function failed to create context."
                " GPU instrumentation cannot continue. Please report bug.\n");
        return TOOL_REGISTER_ERROR;
    }

    /* Start profiling */
    CHECK_ROCPROFILER(rocprofiler_start_context(_ctx));

    rocprofiler_sdk_plugin_started = true;

    return TOOL_REGISTER_SUCCESS;
}

static void tool_fini(void* tool_data) {

    if (rocprofiler_sdk_plugin_started) {

        /* Flush now and send measurements to TALP */
        rocprofiler_sdk_backend_flush();

        /* Stop measurements (usually already stopped by rocprofiler-sdk finalization) */
        int active_ctx = 0;
        rocprofiler_status_t status = rocprofiler_context_is_active(_ctx, &active_ctx);
        if (status == ROCPROFILER_STATUS_SUCCESS && active_ctx != 0) {
            CHECK_WARN_ROCPROFILER(rocprofiler_stop_context(_ctx));
        }

        rocprofiler_sdk_plugin_started = false;
    }
}

DLB_EXPORT_SYMBOL
rocprofiler_tool_configure_result_t*
rocprofiler_configure(uint32_t                 version,
                      const char*              version_string,
                      uint32_t                 priority,
                      rocprofiler_client_id_t* id) {

    client_id = id;

    id->name = "DLB rocprofiler-sdk plugin";

    uint32_t major = version / 10000;
    uint32_t minor = (version % 10000) / 100;
    uint32_t patch = version % 100;
    PLUGIN_PRINT("%s (priority=%"PRIu32")"
           " is using rocprofiler-sdk v%"PRIu32".%"PRIu32".%"PRIu32" (%s)\n",
           id->name, priority, major, minor, patch, version_string);

    static rocprofiler_tool_configure_result_t cfg = {
        sizeof(rocprofiler_tool_configure_result_t),
        tool_init,
        tool_fini,
        NULL
    };

    return &cfg;
}
