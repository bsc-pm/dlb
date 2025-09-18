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

#include "plugins/plugin.h"
#include "plugins/plugin_verbose.h"
#include "plugins/gpu_record_utils.h"
#include "support/dlb_common.h"
#include "talp/talp_gpu.h"
#include <inttypes.h>
#include <rocprofiler/v2/rocprofiler.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_ROCPROFILER(call)                                                 \
    do {                                                                        \
        rocprofiler_status_t _status = (call);                                  \
        if (_status != ROCPROFILER_STATUS_SUCCESS) {                            \
            const char *_error_string = rocprofiler_error_str(_status);         \
            fprintf(stderr,                                                     \
                    "%s:%d: Error: Function %s failed with error (%d): %s.\n",  \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
                                                                                \
            return DLB_PLUGIN_ERROR;                                            \
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


#define ROCPROFILER_BUFFER_SIZE (2 * 1024 * 1024)   // 2MB
#define INTERMEDIATE_BUFFER_SIZE (16 * 1024)        // * 16 bytes/element =  ~256 KB


static bool rocprofilerv2_plugin_enabled = false;
static atomic_uint_least64_t computation_time = 0;
static atomic_uint_least64_t communication_time = 0;
static rocprofiler_session_id_t _session_id;
static rocprofiler_buffer_id_t _buffer_id;

/* Callback for the HIP API called by the host */
static void HIP_API_callback(rocprofiler_record_tracer_t tracer_record,
        rocprofiler_session_id_t session_id) {

    if (tracer_record.domain == ACTIVITY_DOMAIN_HIP_API) {
        if (tracer_record.phase == ROCPROFILER_PHASE_ENTER) {
            talp_gpu_into_runtime_api();
        } else if (tracer_record.phase == ROCPROFILER_PHASE_EXIT) {
            talp_gpu_out_of_runtime_api();
        }
    } else {
        PLUGIN_PRINT("unknown HIP callback from domain: %d\n", tracer_record.domain);
    }
}

/* Called by ROCPROFILER when buffer needs to be flushed. Updates current
 * computation_time and communication_time*/
static void buffer_callback(const rocprofiler_record_header_t* begin,
                     const rocprofiler_record_header_t* end,
                     rocprofiler_session_id_t session_id,
                     rocprofiler_buffer_id_t buffer_id) {

    /* Intermediate buffers for computing time */
    int num_kernel_records = 0;
    gpu_record_t kernel_records_buffer[INTERMEDIATE_BUFFER_SIZE];

    int num_memory_records = 0;
    gpu_record_t memory_records_buffer[INTERMEDIATE_BUFFER_SIZE];

    while (begin < end) {
        if (!begin) break;
        switch (begin->kind) {
            case ROCPROFILER_PROFILER_RECORD: {
                const rocprofiler_record_profiler_t* profiler_record =
                    (const rocprofiler_record_profiler_t*)begin;

                uint64_t kernel_start = profiler_record->timestamps.begin.value;
                uint64_t kernel_end = profiler_record->timestamps.end.value;

                kernel_records_buffer[num_kernel_records++] = (gpu_record_t){
                    .start = kernel_start,
                    .end = kernel_end,
                };

                PLUGIN_PRINT("KERNEL: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        kernel_start, kernel_end, kernel_end - kernel_start);

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

                    memory_records_buffer[num_memory_records++] = (gpu_record_t){
                        .start = memory_start,
                        .end = memory_end,
                    };

                    PLUGIN_PRINT("%s: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            operation_name,
                            memory_start, memory_end, memory_end - memory_start);
                }
                break;
            }
            default:
                PLUGIN_PRINT("unknown record with kind=%d\n", begin->kind);
        }
        rocprofiler_next_record(begin, &begin, session_id, buffer_id);
    }

    /* Compute kernel records duration */
    int new_num_kernel_records = gpu_record_clean_and_merge(
            kernel_records_buffer, num_kernel_records);
    uint64_t kernel_time = gpu_record_get_duration(kernel_records_buffer, new_num_kernel_records);
    atomic_fetch_add_explicit(&computation_time, kernel_time, memory_order_relaxed);

    /* Compute memory records duration */
    int new_num_memory_records = gpu_record_clean_and_merge(
            memory_records_buffer, num_memory_records);
    uint64_t memory_time = gpu_record_get_memory_exclusive_duration(
            memory_records_buffer, new_num_memory_records,
            kernel_records_buffer, new_num_kernel_records);
    atomic_fetch_add_explicit(&communication_time, memory_time, memory_order_relaxed);

    PLUGIN_PRINT("computed sample, computation: %"PRIu64", communication: %"PRIu64"\n",
            kernel_time, memory_time);
}


static void rocprofilerv2_plugin_update_sample(void) {

    /* Flush buffer */
    if (rocprofilerv2_plugin_enabled) {
        // TODO: is this a blocking function or do we need to sync with buffer_callback
        CHECK_WARN_ROCPROFILER(rocprofiler_flush_data(_session_id, _buffer_id));
    }

    /* Flush timers for next sample */
    uint64_t gpu_useful_time = atomic_exchange_explicit(
            &computation_time, 0, memory_order_acquire);
    uint64_t gpu_communication_time = atomic_exchange_explicit(
            &communication_time, 0, memory_order_acquire);

    if (gpu_useful_time > 0 || gpu_communication_time > 0) {
        /* Pack values to send to TALP */
        talp_gpu_measurements_t measurements = {
            .useful_time = gpu_useful_time,
            .communication_time = gpu_communication_time,
        };

        /* Call TALP */
        talp_gpu_update_sample(measurements);
    }
}

static int rocprofilerv2_plugin_init(plugin_info_t *info) {

    CHECK_ROCPROFILER(rocprofiler_initialize());

    CHECK_ROCPROFILER(rocprofiler_create_session(ROCPROFILER_NONE_REPLAY_MODE, &_session_id));

    /* Create buffer. It will be only used for asynchronous operations, for now:
     * API filter for HIP operations and Kernel Tracing Filter */
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

    /* Start profiling */
    CHECK_ROCPROFILER(rocprofiler_start_session(_session_id));

    /* Enable GPU component in TALP */
    talp_gpu_init(rocprofilerv2_plugin_update_sample);

    /* Return some useful information */
    info->name = "rocprofilerv2";
    info->version = 1;

    rocprofilerv2_plugin_enabled = true;

    return DLB_PLUGIN_SUCCESS;
}

static int rocprofilerv2_plugin_finalize(void) {

    if (rocprofilerv2_plugin_enabled) {

        /* Finalize TALP GPU first to compute unflushed records */
        talp_gpu_finalize();

        CHECK_WARN_ROCPROFILER(rocprofiler_terminate_session(_session_id));
        CHECK_WARN_ROCPROFILER(rocprofiler_destroy_session(_session_id));
        CHECK_WARN_ROCPROFILER(rocprofiler_finalize());

        rocprofilerv2_plugin_enabled = false;
    }

    return DLB_PLUGIN_SUCCESS;
}


DLB_EXPORT_SYMBOL
plugin_api_t* DLB_Get_Plugin_API(void) {
    static plugin_api_t api = {
        .init = rocprofilerv2_plugin_init,
        .finalize = rocprofilerv2_plugin_finalize,
    };
    return &api;
}
