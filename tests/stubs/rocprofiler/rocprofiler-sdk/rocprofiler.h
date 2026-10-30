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

// This file contains portions derived from rocprofiler-sdk library.
// Original copyright:

// MIT License
//
// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ROCPROFILER_H
#define ROCPROFILER_H

#include <stdint.h>

typedef enum
{
    ROCPROFILER_STATUS_SUCCESS = 0,
    ROCPROFILER_STATUS_ERROR,
    ROCPROFILER_STATUS_ERROR_INCOMPATIBLE_ABI,
    ROCPROFILER_STATUS_ERROR_INVALID_ARGUMENT,
} rocprofiler_status_t;

typedef enum
{
    ROCPROFILER_AGENT_INFO_VERSION_NONE = 0,
    ROCPROFILER_AGENT_INFO_VERSION_0    = 1,
    ROCPROFILER_AGENT_INFO_VERSION_LAST,
} rocprofiler_agent_version_t;

typedef enum
{
    ROCPROFILER_AGENT_TYPE_NONE = 0,
    ROCPROFILER_AGENT_TYPE_CPU,
    ROCPROFILER_AGENT_TYPE_GPU,
    ROCPROFILER_AGENT_TYPE_LAST,
} rocprofiler_agent_type_t;

typedef enum
{
    ROCPROFILER_BUFFER_POLICY_NONE = 0,
    ROCPROFILER_BUFFER_POLICY_DISCARD,
    ROCPROFILER_BUFFER_POLICY_LOSSLESS,
    ROCPROFILER_BUFFER_POLICY_LAST,
} rocprofiler_buffer_policy_t;

typedef enum
{
    ROCPROFILER_BUFFER_TRACING_NONE = 0,
    ROCPROFILER_BUFFER_TRACING_HSA_CORE_API,
    ROCPROFILER_BUFFER_TRACING_HSA_AMD_EXT_API,
    ROCPROFILER_BUFFER_TRACING_HSA_IMAGE_EXT_API,
    ROCPROFILER_BUFFER_TRACING_HSA_FINALIZE_EXT_API,
    ROCPROFILER_BUFFER_TRACING_HIP_RUNTIME_API,
    ROCPROFILER_BUFFER_TRACING_HIP_COMPILER_API,
    ROCPROFILER_BUFFER_TRACING_MARKER_CORE_API,
    ROCPROFILER_BUFFER_TRACING_MARKER_CONTROL_API,
    ROCPROFILER_BUFFER_TRACING_MARKER_NAME_API,
    ROCPROFILER_BUFFER_TRACING_MEMORY_COPY,
    ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
    ROCPROFILER_BUFFER_TRACING_PAGE_MIGRATION,
    ROCPROFILER_BUFFER_TRACING_SCRATCH_MEMORY,
    ROCPROFILER_BUFFER_TRACING_CORRELATION_ID_RETIREMENT,
    ROCPROFILER_BUFFER_TRACING_RCCL_API,
    ROCPROFILER_BUFFER_TRACING_LAST,
} rocprofiler_buffer_tracing_kind_t;

typedef enum
{
    ROCPROFILER_CALLBACK_TRACING_NONE = 0,
    ROCPROFILER_CALLBACK_TRACING_HSA_CORE_API,
    ROCPROFILER_CALLBACK_TRACING_HSA_AMD_EXT_API,
    ROCPROFILER_CALLBACK_TRACING_HSA_IMAGE_EXT_API,
    ROCPROFILER_CALLBACK_TRACING_HSA_FINALIZE_EXT_API,
    ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
    ROCPROFILER_CALLBACK_TRACING_HIP_COMPILER_API,
    ROCPROFILER_CALLBACK_TRACING_MARKER_CORE_API,
    ROCPROFILER_CALLBACK_TRACING_MARKER_CONTROL_API,
    ROCPROFILER_CALLBACK_TRACING_MARKER_NAME_API,
    ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
    ROCPROFILER_CALLBACK_TRACING_SCRATCH_MEMORY,
    ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
    ROCPROFILER_CALLBACK_TRACING_MEMORY_COPY,
    ROCPROFILER_CALLBACK_TRACING_RCCL_API,
    ROCPROFILER_CALLBACK_TRACING_LAST,
} rocprofiler_callback_tracing_kind_t;

typedef enum
{
    ROCPROFILER_CALLBACK_PHASE_NONE = 0,
    ROCPROFILER_CALLBACK_PHASE_ENTER,
    ROCPROFILER_CALLBACK_PHASE_LOAD =
        ROCPROFILER_CALLBACK_PHASE_ENTER,
    ROCPROFILER_CALLBACK_PHASE_EXIT,
    ROCPROFILER_CALLBACK_PHASE_UNLOAD =
        ROCPROFILER_CALLBACK_PHASE_EXIT,
    ROCPROFILER_CALLBACK_PHASE_LAST,
} rocprofiler_callback_phase_t;

typedef enum
{
    ROCPROFILER_MEMORY_COPY_NONE = 0,
    ROCPROFILER_MEMORY_COPY_HOST_TO_HOST,
    ROCPROFILER_MEMORY_COPY_HOST_TO_DEVICE,
    ROCPROFILER_MEMORY_COPY_DEVICE_TO_HOST,
    ROCPROFILER_MEMORY_COPY_DEVICE_TO_DEVICE,
    ROCPROFILER_MEMORY_COPY_LAST,
} rocprofiler_memory_copy_operation_t;

typedef struct {
    uint64_t handle;
} rocprofiler_agent_id_t;

typedef struct {
    uint64_t handle;
} rocprofiler_context_id_t;

typedef struct {
    uint64_t handle;
} rocprofiler_buffer_id_t;

typedef struct {
    uint64_t handle;
} rocprofiler_callback_thread_t;

typedef struct rocprofiler_agent_t {
    rocprofiler_agent_id_t id;
    rocprofiler_agent_type_t type;
    uint32_t location_id;
    uint32_t domain;
    int32_t logical_node_type_id;
} rocprofiler_agent_t;

typedef int32_t rocprofiler_tracing_operation_t;

typedef struct rocprofiler_callback_tracing_record_t
{
    /* rocprofiler_context_id_t            context_id; */
    /* rocprofiler_thread_id_t             thread_id; */
    /* rocprofiler_correlation_id_t        correlation_id; */
    rocprofiler_callback_tracing_kind_t kind;
    rocprofiler_tracing_operation_t     operation;
    rocprofiler_callback_phase_t        phase;
    /* void*                               payload; */
} rocprofiler_callback_tracing_record_t;

typedef union rocprofiler_user_data_t
{
    uint64_t value;
    void*    ptr;
} rocprofiler_user_data_t;

typedef struct
{
    union
    {
        struct
        {
            uint32_t category;
            uint32_t kind;
        };
        uint64_t hash;
    };
    void* payload;
} rocprofiler_record_header_t;

typedef uint64_t rocprofiler_timestamp_t;

typedef struct rocprofiler_kernel_dispatch_info_t {
    rocprofiler_agent_id_t    agent_id;
} rocprofiler_kernel_dispatch_info_t;

typedef struct rocprofiler_buffer_tracing_kernel_dispatch_record_t {
    rocprofiler_timestamp_t            start_timestamp;
    rocprofiler_timestamp_t            end_timestamp;
    rocprofiler_kernel_dispatch_info_t dispatch_info;
} rocprofiler_buffer_tracing_kernel_dispatch_record_t;

typedef struct rocprofiler_buffer_tracing_memory_copy_record_t {
    rocprofiler_memory_copy_operation_t operation;
    rocprofiler_timestamp_t             start_timestamp;
    rocprofiler_timestamp_t             end_timestamp;
    rocprofiler_agent_id_t              dst_agent_id;
    rocprofiler_agent_id_t              src_agent_id;
} rocprofiler_buffer_tracing_memory_copy_record_t;

typedef union {
    rocprofiler_buffer_tracing_kernel_dispatch_record_t kernel;
    rocprofiler_buffer_tracing_memory_copy_record_t     memory;
} rocprofiler_record_t;


typedef rocprofiler_status_t (*rocprofiler_query_available_agents_cb_t)(
    rocprofiler_agent_version_t version,
    const void**                agents,
    size_t                      num_agents,
    void*                       user_data);

typedef void (*rocprofiler_buffer_tracing_cb_t)(rocprofiler_context_id_t      context,
                                                rocprofiler_buffer_id_t       buffer_id,
                                                rocprofiler_record_header_t** headers,
                                                size_t                        num_headers,
                                                void*                         data,
                                                uint64_t                      drop_count);

typedef void (*rocprofiler_callback_tracing_cb_t)(rocprofiler_callback_tracing_record_t record,
                                                  rocprofiler_user_data_t*              user_data,
                                                  void* callback_data);


rocprofiler_status_t
rocprofiler_query_available_agents(rocprofiler_agent_version_t             version,
                                   rocprofiler_query_available_agents_cb_t callback,
                                   size_t                                  agent_size,
                                   void* user_data);

rocprofiler_status_t
rocprofiler_create_context(rocprofiler_context_id_t* context_id);

rocprofiler_status_t
rocprofiler_start_context(rocprofiler_context_id_t context_id);

rocprofiler_status_t
rocprofiler_stop_context(rocprofiler_context_id_t context_id);

rocprofiler_status_t
rocprofiler_context_is_valid(rocprofiler_context_id_t context_id, int* status);

rocprofiler_status_t
rocprofiler_context_is_active(rocprofiler_context_id_t context_id, int* status);

rocprofiler_status_t
rocprofiler_configure_callback_tracing_service(rocprofiler_context_id_t               context_id,
                                               rocprofiler_callback_tracing_kind_t    kind,
                                               const rocprofiler_tracing_operation_t* operations,
                                               size_t                            operations_count,
                                               rocprofiler_callback_tracing_cb_t callback,
                                               void* callback_args);

rocprofiler_status_t
rocprofiler_query_callback_tracing_kind_operation_name(rocprofiler_callback_tracing_kind_t kind,
                                                       rocprofiler_tracing_operation_t operation,
                                                       const char**                    name,
                                                       uint64_t* name_len);

rocprofiler_status_t
rocprofiler_create_buffer(rocprofiler_context_id_t        context,
                          size_t                          size,
                          size_t                          watermark,
                          rocprofiler_buffer_policy_t     policy,
                          rocprofiler_buffer_tracing_cb_t callback,
                          void*                           callback_data,
                          rocprofiler_buffer_id_t*        buffer_id);

rocprofiler_status_t
rocprofiler_configure_buffer_tracing_service(rocprofiler_context_id_t               context_id,
                                             rocprofiler_buffer_tracing_kind_t      kind,
                                             const rocprofiler_tracing_operation_t* operations,
                                             size_t                  operations_count,
                                             rocprofiler_buffer_id_t buffer_id);
rocprofiler_status_t
rocprofiler_flush_buffer(rocprofiler_buffer_id_t buffer_id);

rocprofiler_status_t
rocprofiler_create_callback_thread(rocprofiler_callback_thread_t* cb_thread_id);

rocprofiler_status_t
rocprofiler_assign_callback_thread(rocprofiler_buffer_id_t       buffer_id,
                                   rocprofiler_callback_thread_t cb_thread_id);

const char* rocprofiler_get_status_string(rocprofiler_status_t status);

#endif /* ROCPROFILER_H */
