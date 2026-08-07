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

/* This file contains portions derived from rocprofilerv2 library.
 * Original copyright: */

/******************************************************************************
Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*******************************************************************************/

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ROCPROFILER_STATUS_SUCCESS = 0,
    ROCPROFILER_STATUS_ERROR = -1,
    ROCPROFILER_STATUS_ERROR_ALREADY_INITIALIZED = -2,
    ROCPROFILER_STATUS_ERROR_NOT_INITIALIZED = -3,
    ROCPROFILER_STATUS_ERROR_SESSION_MISSING_BUFFER = -4,
    ROCPROFILER_STATUS_ERROR_TIMESTAMP_NOT_APPLICABLE = -5,
    ROCPROFILER_STATUS_ERROR_AGENT_NOT_FOUND = -6,
    ROCPROFILER_STATUS_ERROR_AGENT_INFORMATION_MISSING = -7,
    ROCPROFILER_STATUS_ERROR_QUEUE_NOT_FOUND = -8,
    ROCPROFILER_STATUS_ERROR_QUEUE_INFORMATION_MISSING = -9,
    ROCPROFILER_STATUS_ERROR_KERNEL_NOT_FOUND = -10,
    ROCPROFILER_STATUS_ERROR_KERNEL_INFORMATION_MISSING = -11,
    ROCPROFILER_STATUS_ERROR_COUNTER_NOT_FOUND = -12,
    ROCPROFILER_STATUS_ERROR_COUNTER_INFORMATION_MISSING = -13,
    ROCPROFILER_STATUS_ERROR_TRACER_API_DATA_NOT_FOUND = -14,
    ROCPROFILER_STATUS_ERROR_TRACER_API_DATA_INFORMATION_MISSING = -15,
    ROCPROFILER_STATUS_ERROR_INCORRECT_DOMAIN = -16,
    ROCPROFILER_STATUS_ERROR_SESSION_NOT_FOUND = -17,
    ROCPROFILER_STATUS_ERROR_CORRUPTED_SESSION_BUFFER = -18,
    ROCPROFILER_STATUS_ERROR_RECORD_CORRUPTED = -19,
    ROCPROFILER_STATUS_ERROR_INCORRECT_REPLAY_MODE = -20,
    ROCPROFILER_STATUS_ERROR_SESSION_MISSING_FILTER = -21,
    ROCPROFILER_STATUS_ERROR_INCORRECT_SIZE = -22,
    ROCPROFILER_STATUS_ERROR_INCORRECT_FLUSH_INTERVAL = -23,
    ROCPROFILER_STATUS_ERROR_SESSION_FILTER_DATA_MISMATCH = -24,
    ROCPROFILER_STATUS_ERROR_FILTER_DATA_CORRUPTED = -25,
    ROCPROFILER_STATUS_ERROR_CORRUPTED_LABEL_DATA = -26,
    ROCPROFILER_STATUS_ERROR_RANGE_STACK_IS_EMPTY = -27,
    ROCPROFILER_STATUS_ERROR_PASS_NOT_STARTED = -28,
    ROCPROFILER_STATUS_ERROR_HAS_ACTIVE_SESSION = -29,
    ROCPROFILER_STATUS_ERROR_SESSION_NOT_ACTIVE = -30,
    ROCPROFILER_STATUS_ERROR_FILTER_NOT_FOUND = -31,
    ROCPROFILER_STATUS_ERROR_BUFFER_NOT_FOUND = -32,
    ROCPROFILER_STATUS_ERROR_FILTER_NOT_SUPPORTED = -33,
    ROCPROFILER_STATUS_ERROR_INVALID_ARGUMENTS = -34,
    ROCPROFILER_STATUS_ERROR_INVALID_OPERATION_ID = -35,
    ROCPROFILER_STATUS_ERROR_INVALID_DOMAIN_ID = -36,
    ROCPROFILER_STATUS_ERROR_NOT_IMPLEMENTED = -37,
    ROCPROFILER_STATUS_ERROR_MISMATCHED_EXTERNAL_CORRELATION_ID = -38,
} rocprofiler_status_t;

typedef enum {
    ACTIVITY_DOMAIN_HSA_API = 0,
    ACTIVITY_DOMAIN_HSA_OPS = 1,
    ACTIVITY_DOMAIN_HIP_OPS = 2,
    ACTIVITY_DOMAIN_HIP_API = 3,
    ACTIVITY_DOMAIN_KFD_API = 4,
    ACTIVITY_DOMAIN_EXT_API = 5,
    ACTIVITY_DOMAIN_ROCTX = 6,
    ACTIVITY_DOMAIN_HSA_EVT = 7,
    ACTIVITY_DOMAIN_NUMBER
} rocprofiler_tracer_activity_domain_t;

typedef enum {
    ROCPROFILER_PROFILER_RECORD = 0,
    ROCPROFILER_TRACER_RECORD = 1,
    ROCPROFILER_ATT_TRACER_RECORD = 2,
    ROCPROFILER_PC_SAMPLING_RECORD = 3,
    ROCPROFILER_SPM_RECORD = 4,
    ROCPROFILER_COUNTERS_SAMPLER_RECORD = 5
} rocprofiler_record_kind_t;

typedef enum {
    ROCPROFILER_NONE_REPLAY_MODE = -1,
} rocprofiler_replay_mode_t;

typedef enum {
    ROCPROFILER_PHASE_NONE = 0,
    ROCPROFILER_PHASE_ENTER = 1,
    ROCPROFILER_PHASE_EXIT = 2
} rocprofiler_api_tracing_phase_t;

typedef enum {
    ROCPROFILER_DISPATCH_TIMESTAMPS_COLLECTION = 1,
    ROCPROFILER_COUNTERS_COLLECTION = 2,
    ROCPROFILER_PC_SAMPLING_COLLECTION = 3,
    ROCPROFILER_ATT_TRACE_COLLECTION = 4,
    ROCPROFILER_SPM_COLLECTION = 5,
    ROCPROFILER_API_TRACE = 6,
    ROCPROFILER_COUNTERS_SAMPLER = 7
} rocprofiler_filter_kind_t;

typedef struct {
    uint64_t handle;
} rocprofiler_session_id_t;

typedef struct {
    uint64_t value;
} rocprofiler_buffer_id_t;

typedef struct {
    uint64_t value;
} rocprofiler_filter_id_t;

typedef struct {
    uint64_t handle;
} rocprofiler_record_id_t;

typedef struct {
    uint32_t id;
} rocprofiler_tracer_operation_id_t;

typedef struct {
    uint64_t value;
} rocprofiler_timestamp_t;

typedef struct {
    uint64_t handle;
} rocprofiler_agent_id_t;

typedef struct {
    rocprofiler_record_kind_t kind;
    rocprofiler_record_id_t id;
} rocprofiler_record_header_t;

typedef struct {
  rocprofiler_timestamp_t begin;
  rocprofiler_timestamp_t end;
} rocprofiler_record_header_timestamp_t;

typedef struct {
    rocprofiler_record_header_t header;
    /* rocprofiler_tracer_external_id_t external_id; */
    rocprofiler_tracer_activity_domain_t domain;
    rocprofiler_tracer_operation_id_t operation_id;
    /* rocprofiler_tracer_api_data_t api_data; */
    /* rocprofiler_tracer_activity_correlation_id_t correlation_id; */
    rocprofiler_record_header_timestamp_t timestamps;
    /* rocprofiler_agent_id_t agent_id; */
    /* rocprofiler_queue_id_t queue_id; */
    /* rocprofiler_thread_id_t thread_id; */
    rocprofiler_api_tracing_phase_t phase;
    /* const char* name; */
} rocprofiler_record_tracer_t;

typedef struct {
    rocprofiler_record_header_t header;
    /* rocprofiler_kernel_id_t kernel_id; */
    /* rocprofiler_agent_id_t gpu_id; */
    /* rocprofiler_queue_id_t queue_id; */
    rocprofiler_record_header_timestamp_t timestamps;
    /* const rocprofiler_record_counter_instance_t* counters; */
    /* rocprofiler_record_counters_instances_count_t counters_count; */
    /* uint32_t xcc_index; */
    /* rocprofiler_kernel_properties_t kernel_properties; */
    /* rocprofiler_thread_id_t thread_id; */
    /* rocprofiler_queue_index_t queue_idx; */
    /* rocprofiler_correlation_id_t correlation_id; */
} rocprofiler_record_profiler_t;

typedef union {
    rocprofiler_tracer_activity_domain_t* trace_apis;
    const char** counters_names;
    /* rocprofiler_att_parameter_t* att_parameters; */
    /* rocprofiler_spm_parameter_t* spm_parameters; */
    /* rocprofiler_counters_sampler_parameters_t counters_sampler_parameters; */
} rocprofiler_filter_data_t;

typedef struct {
  /* rocprofiler_filter_property_kind_t kind; */
  /* union { */
  /*   const char** name_regex; */
  /*   rocprofiler_hip_function_name_t* hip_functions_names; */
  /*   rocprofiler_hsa_function_name_t* hsa_functions_names; */
  /*   uint32_t range[2]; */
  /*   struct { */
  /*     uint64_t start; */
  /*     uint64_t end; */
  /*   }* dispatch_ids; */
  /* }; */
  uint64_t data_count;
} rocprofiler_filter_property_t;

typedef void (*rocprofiler_buffer_callback_t)(const rocprofiler_record_header_t* begin,
                                              const rocprofiler_record_header_t* end,
                                              rocprofiler_session_id_t session_id,
                                              rocprofiler_buffer_id_t buffer_id);

typedef void (*rocprofiler_sync_callback_t)(rocprofiler_record_tracer_t record,
                                            rocprofiler_session_id_t session_id);


rocprofiler_status_t rocprofiler_initialize(void);
rocprofiler_status_t rocprofiler_finalize(void);

rocprofiler_status_t rocprofiler_create_session(rocprofiler_replay_mode_t replay_mode,
                           rocprofiler_session_id_t* session_id);
rocprofiler_status_t rocprofiler_start_session(rocprofiler_session_id_t session_id);
rocprofiler_status_t rocprofiler_terminate_session(rocprofiler_session_id_t session_id);
rocprofiler_status_t rocprofiler_destroy_session(rocprofiler_session_id_t session_id);

rocprofiler_status_t rocprofiler_create_buffer(
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_callback_t buffer_callback,
        size_t buffer_size,
        rocprofiler_buffer_id_t* buffer_id);

rocprofiler_status_t rocprofiler_create_filter(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_kind_t filter_kind,
        rocprofiler_filter_data_t data,
        uint64_t data_count,
        rocprofiler_filter_id_t* filter_id,
        rocprofiler_filter_property_t property);

rocprofiler_status_t rocprofiler_set_filter_buffer(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_id_t filter_id,
        rocprofiler_buffer_id_t buffer_id);

rocprofiler_status_t rocprofiler_set_api_trace_sync_callback(
        rocprofiler_session_id_t session_id,
        rocprofiler_filter_id_t filter_id,
        rocprofiler_sync_callback_t callback);

rocprofiler_status_t rocprofiler_flush_data(
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_id_t buffer_id);

rocprofiler_status_t rocprofiler_next_record(
        const rocprofiler_record_header_t* record,
        const rocprofiler_record_header_t** next,
        rocprofiler_session_id_t session_id,
        rocprofiler_buffer_id_t buffer_id);

rocprofiler_status_t rocprofiler_query_tracer_operation_name(
        rocprofiler_tracer_activity_domain_t domain,
        rocprofiler_tracer_operation_id_t operation_id,
        const char** name);

const char* rocprofiler_error_str(rocprofiler_status_t status);

#ifndef ROCPROFILER_H
#define ROCPROFILER_H
#endif /* ROCPROFILER_H */
