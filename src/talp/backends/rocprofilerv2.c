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

#include <inttypes.h>
#include <rocprofiler/v2/rocprofiler.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static const core_api_t *dlb_core_api = NULL;

/*********************************************************************************/
/*  ROCm profiling (rocprofilerv2)                                               */
/*********************************************************************************/


#define CHECK_ROCPROFILER(call)                                                 \
    do {                                                                        \
        rocprofiler_status_t _status = (call);                                  \
        if (_status != ROCPROFILER_STATUS_SUCCESS) {                            \
            const char *_error_string = rocprofiler_error_str(_status);         \
            fprintf(stderr,                                                     \
                    "%s:%d: Error: Function %s failed with error (%d): %s.\n",  \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
                                                                                \
            return DLB_BACKEND_ERROR;                                           \
        }                                                                       \
  } while (0)

#define CHECK_WARN_ROCPROFILER(call)                                            \
    do {                                                                        \
        rocprofiler_status_t _status = (call);                                  \
        if (_status != ROCPROFILER_STATUS_SUCCESS) {                            \
            const char *_error_string = rocprofiler_error_str(_status);         \
            fprintf(stderr,                                                     \
                    "%s:%d: Warning: Function %s failed with error (%d): %s.\n",\
                    __FILE__, __LINE__, #call, _status, _error_string);         \
        }                                                                       \
  } while (0)


/* --- ROCm state -------------------------------------------------------------- */

static bool rocprofilerv2_plugin_initialized = false;
static bool rocprofilerv2_plugin_started = false;
static rocprofiler_session_id_t _session_id;
static rocprofiler_buffer_id_t _buffer_id;


/* --- Host runtime events ----------------------------------------------------- */

/* Callback for the HIP API called by the host */
static void HIP_API_callback(rocprofiler_record_tracer_t tracer_record,
        rocprofiler_session_id_t session_id) {

    if (tracer_record.domain == ACTIVITY_DOMAIN_HIP_API) {
        const char* function_name = NULL;
        if (plugin_is_verbose()) {
            CHECK_WARN_ROCPROFILER(rocprofiler_query_tracer_operation_name(
                        tracer_record.domain, tracer_record.operation_id, &function_name));
        }
        if (tracer_record.phase == ROCPROFILER_PHASE_ENTER) {
            PLUGIN_PRINT(" >> %s\n", function_name);
            dlb_core_api->gpu.enter_runtime();
        } else if (tracer_record.phase == ROCPROFILER_PHASE_EXIT) {
            PLUGIN_PRINT(" << %s\n", function_name);
            dlb_core_api->gpu.exit_runtime();
        }
    } else {
        PLUGIN_PRINT("unknown HIP callback from domain: %d\n", tracer_record.domain);
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

/* After flushing the buffers, advance safe_timestamp so that any future records
 * with start times earlier than this are ignored. */
static uint64_t safe_timestamp = 0;


/* Called by ROCPROFILER when a buffer needs to be flushed.
 * The callback receives a contiguous range of activity records [begin, end].
 * We copy only the minimal information we need (start and end timestamps)
 * into our own buffers for later processing.
 * This callback is invoked serially per buffer. Since we only define one
 * session and one buffer, the function does not need to be thread-safe. */
static void buffer_callback(const rocprofiler_record_header_t* begin,
                     const rocprofiler_record_header_t* end,
                     rocprofiler_session_id_t session_id,
                     rocprofiler_buffer_id_t buffer_id) {

    while (begin < end) {
        if (!begin) break;
        switch (begin->kind) {
            case ROCPROFILER_PROFILER_RECORD: {
                const rocprofiler_record_profiler_t* profiler_record =
                    (const rocprofiler_record_profiler_t*)begin;

                uint64_t kernel_start = profiler_record->timestamps.begin.value;
                uint64_t kernel_end = profiler_record->timestamps.end.value;

                if (kernel_start >= safe_timestamp
                        && kernel_end > safe_timestamp) {

                    gpu_record_append_event(&kernel_buffer, kernel_start, kernel_end);

                    PLUGIN_PRINT("KERNEL: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            kernel_start, kernel_end, kernel_end - kernel_start);
                }

                break;
            }
            case ROCPROFILER_TRACER_RECORD: {
                const rocprofiler_record_tracer_t* tracer_record =
                    (const rocprofiler_record_tracer_t*)begin;

                /*************************************************************
                 * Memory operation classification
                 *
                 * Currently, the only reliable way we've found to distinguish
                 * memory operations from other HIP operations is by checking
                 * the string returned by rocprofiler_query_tracer_operation_name.
                 *
                 * That function can return many operation names. We separate
                 * them into "memory" and "non-memory", but for our purposes
                 * we only track a subset of memory operations. The selection
                 * is somewhat arbitrary, we focus mainly on copy-like operations
                 * that are typically costly. We may revisit this list in the
                 * future if we observe cases where additional operations need
                 * to be included or excluded.
                 *
                 * Memory operations considered (tracked):
                 *   - CopyDeviceToHost
                 *   - CopyHostToDevice
                 *   - CopyDeviceToDevice
                 *   - CopyDeviceToHost2D
                 *   - CopyHostToDevice2D
                 *   - CopyDeviceToDevice2D
                 *   - CopyImage
                 *   - CopyImageToBuffer
                 *   - CopyBufferToImage
                 *   - MigrateMemObjects
                 *   - SvmMemcpy
                 *
                 * Memory operations ignored (not tracked):
                 *   - FillBuffer / MapBuffer
                 *   - MapImage
                 *   - UnmapMemObject
                 *   - SvmMemFill / SvmMap / SvmUnmap
                 *
                 * Non-memory operations (all ignored):
                 *   - KernelExecution
                 *   - NativeKernel
                 *   - Task
                 *   - Marker / InternalMarker
                 *   - Barrier / StreamWait / StreamWrite
                 *   - User
                 *************************************************************/

                /* Check whether this record is a memory operation that we want to track */
                const char* operation_name = NULL;
                rocprofiler_status_t status = rocprofiler_query_tracer_operation_name(
                        tracer_record->domain, tracer_record->operation_id, &operation_name);
                if (status == ROCPROFILER_STATUS_SUCCESS
                        && operation_name != NULL
                        && (strncmp(operation_name, "Copy", 4) == 0
                            || strcmp(operation_name, "MigrateMemObjects") == 0
                            || strcmp(operation_name, "SvmMemcpy") == 0)
                   ) {

                    uint64_t memory_start = tracer_record->timestamps.begin.value;
                    uint64_t memory_end = tracer_record->timestamps.end.value;

                    if (memory_start >= safe_timestamp
                            && memory_end > safe_timestamp) {

                        gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                        PLUGIN_PRINT("%s: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                                operation_name,
                                memory_start, memory_end, memory_end - memory_start);
                    }
                }
                break;
            }
            default:
                PLUGIN_PRINT("unknown record with kind=%d\n", begin->kind);
        }
        rocprofiler_next_record(begin, &begin, session_id, buffer_id);
    }
}


/* --- Plugin functions -------------------------------------------------------- */

/* Function called externally by TALP to force flushing buffers */
static void rocprofilerv2_backend_flush(void) {

    /* Flush buffer */
    if (rocprofilerv2_plugin_started) {
        // TODO: is this a blocking function or do we need to sync with buffer_callback
        CHECK_WARN_ROCPROFILER(rocprofiler_flush_data(_session_id, _buffer_id));
    }

    if (rocprofilerv2_plugin_initialized) {
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

static int rocprofilerv2_backend_probe(const core_api_t *core_api) {
    return DLB_PLUGIN_SUCCESS;
}

static int rocprofilerv2_backend_init(const core_api_t *core_api) {

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION
            || dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    /* Allocate local buffers */
    enum { LOCAL_BUFFER_INITIAL_CAPACITY = 256 * 1024 }; // 256k records = 4MB
    gpu_record_init_buffer(&kernel_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);
    gpu_record_init_buffer(&memory_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);

    CHECK_ROCPROFILER(rocprofiler_initialize());

    CHECK_ROCPROFILER(rocprofiler_create_session(ROCPROFILER_NONE_REPLAY_MODE, &_session_id));

    /* Create buffer. It will be only used for asynchronous operations, for now:
     * API filter for HIP operations and Kernel Tracing Filter */
    enum { ROCPROFILER_BUFFER_SIZE = 2 * 1024 * 1024 }; // 2MB
    CHECK_ROCPROFILER(rocprofiler_create_buffer(_session_id, buffer_callback,
                ROCPROFILER_BUFFER_SIZE, &_buffer_id));

    /* HIP API Filter */
    rocprofiler_filter_id_t api_tracing_filter_id;
    rocprofiler_tracer_activity_domain_t apis_requested[] = {
        ACTIVITY_DOMAIN_HIP_API,
        ACTIVITY_DOMAIN_HIP_OPS,
    };
    enum { num_apis_requested = sizeof(apis_requested) / sizeof(apis_requested[0]) };
    CHECK_ROCPROFILER(rocprofiler_create_filter(
            _session_id,
            ROCPROFILER_API_TRACE,
            (rocprofiler_filter_data_t) {.trace_apis = apis_requested},
            num_apis_requested,
            &api_tracing_filter_id,
            (rocprofiler_filter_property_t) {}));

    CHECK_ROCPROFILER(rocprofiler_set_filter_buffer(
            _session_id, api_tracing_filter_id, _buffer_id));

    CHECK_ROCPROFILER(rocprofiler_set_api_trace_sync_callback(
            _session_id, api_tracing_filter_id, HIP_API_callback));

    /* Kernel Tracing Filter */
    rocprofiler_filter_id_t kernel_tracing_filter_id;
    CHECK_ROCPROFILER(rocprofiler_create_filter(
            _session_id,
            ROCPROFILER_DISPATCH_TIMESTAMPS_COLLECTION,
            (rocprofiler_filter_data_t) {},
            0,
            &kernel_tracing_filter_id,
            (rocprofiler_filter_property_t) {}));

    CHECK_ROCPROFILER(rocprofiler_set_filter_buffer(
            _session_id, kernel_tracing_filter_id, _buffer_id));

    rocprofilerv2_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

static int rocprofilerv2_backend_start(void) {

    if (rocprofilerv2_plugin_initialized) {

        /* Set initial safe timestamp. All records prior to this will be ignored */
        safe_timestamp = get_timestamp();

        /* Start profiling */
        CHECK_ROCPROFILER(rocprofiler_start_session(_session_id));

        rocprofilerv2_plugin_started = true;

        return DLB_BACKEND_SUCCESS;
    }

    return DLB_BACKEND_ERROR;
}

static int rocprofilerv2_backend_stop(void) {

    if (rocprofilerv2_plugin_started) {

        /* Flush now and send measurements to TALP */
        rocprofilerv2_backend_flush();

        /* Stop session */
        CHECK_WARN_ROCPROFILER(rocprofiler_terminate_session(_session_id));

        rocprofilerv2_plugin_started = false;
    }

    return DLB_BACKEND_SUCCESS;
}

static int rocprofilerv2_backend_finalize(void) {

    if (rocprofilerv2_plugin_initialized) {

        /* Finalize rocprofiler */
        CHECK_WARN_ROCPROFILER(rocprofiler_destroy_session(_session_id));
        CHECK_WARN_ROCPROFILER(rocprofiler_finalize());

        /* Free local buffers */
        gpu_record_free_buffer(&kernel_buffer);
        gpu_record_free_buffer(&memory_buffer);

        rocprofilerv2_plugin_initialized = false;
    }

    return DLB_BACKEND_SUCCESS;
}


DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void) {
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "rocprofilerv2",
        .capabilities = {
            .gpu = true,
            .gpu_amd = true,
        },
        .probe = rocprofilerv2_backend_probe,
        .init = rocprofilerv2_backend_init,
        .start = rocprofilerv2_backend_start,
        .start = rocprofilerv2_backend_stop,
        .finalize = rocprofilerv2_backend_finalize,
        .flush = rocprofilerv2_backend_flush,
        .get_gpu_affinity = NULL,
    };
    return &api;
}
