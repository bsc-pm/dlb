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

#include "cuda_runtime.h"
#include "cupti.h"

#include <string.h>
#include <time.h>

/* Host Runtime callback */
static CUpti_CallbackDomain cb_runtime_domain = CUPTI_CB_DOMAIN_INVALID;
static CUpti_CallbackFunc   cb_runtime_fn = NULL;
static void*                cb_runtime_userdata = NULL;

/* Async callbacks */
static CUpti_BuffersCallbackRequestFunc  cb_buffer_request_fn = NULL;
static CUpti_BuffersCallbackCompleteFunc cb_buffer_completed_fn = NULL;

/* Internal functions for buffer management */

typedef struct event_buffer_t {
    uint8_t *base;
    size_t  write_offset;
    size_t  size;
    size_t  max_records;
    size_t  num_records;
} event_buffer_t;


static bool is_buffer_full(const event_buffer_t *buf) {
    return buf->size - buf->write_offset < sizeof(CUpti_Activity)
            || (buf->max_records != 0
                && buf->num_records >= buf->max_records);
}

static event_buffer_t* get_buffer(bool request) {

    // only one buffer for now
    static event_buffer_t single_buffer = {0};

    event_buffer_t *buf = &single_buffer;

    // If buffer is active and we don't need to request
    if (buf->base == NULL && !request) {
        return NULL;
    }

    // Ask for buffer if needed
    if (buf->base == NULL) {
        cb_buffer_request_fn(&buf->base, &buf->size,
                &buf->max_records);

        // First element is reserved for next-element reentrancy:
        size_t write_size = sizeof(CUpti_Activity);
        memset(buf->base, 0, write_size);
        buf->write_offset = write_size;
        buf->num_records = 1;
    }

    // Capacity checks, segfault is ok
    if (is_buffer_full(buf)) {
        return NULL;
    }

    return buf;
}

static void add_record(CUpti_Activity *record) {

    event_buffer_t *buf = get_buffer(true);
    uint8_t *dest = buf->base + buf->write_offset;
    size_t write_size = sizeof(CUpti_Activity);

    // Add
    memcpy(dest, record, write_size);
    buf->write_offset += write_size;
    ++buf->num_records;

    // Flush if needed
    if (is_buffer_full(buf)) {
        cb_buffer_completed_fn((CUcontext){}, 0, buf->base, buf->size, buf->write_offset);
        *buf = (const event_buffer_t){0};
    }
}

static void flush_buffer(void) {

    event_buffer_t *buf = get_buffer(false);
    if (buf != NULL) {
        cb_buffer_completed_fn((CUcontext){}, 0, buf->base, buf->size, buf->write_offset);
        *buf = (const event_buffer_t){0};
    }
}

static uint64_t get_time(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

/* CUDA functions */

cudaError_t cudaGetDevice(int* device) {
    *device = 0;
    return cudaSuccess;
}

cudaError_t cudaGetDeviceCount(int* count) {
    *count = 1;
    return cudaSuccess;
}

cudaError_t cudaGetDeviceProperties(struct cudaDeviceProp *prop, int device) {
    *prop = (const struct cudaDeviceProp){0};
    return cudaSuccess;
}

/* CUPTI functions */

CUptiResult cuptiSubscribe(CUpti_SubscriberHandle *subscriber,
                                    CUpti_CallbackFunc callback,
                                    void *userdata) {
    cb_runtime_fn = callback;
    cb_runtime_userdata = userdata;

    return CUPTI_SUCCESS;
}

CUptiResult cuptiUnsubscribe(CUpti_SubscriberHandle subscriber) {

    cb_runtime_fn = NULL;
    cb_runtime_userdata = NULL;

    return CUPTI_SUCCESS;
}

CUptiResult cuptiFinalize(void) {
    return CUPTI_SUCCESS;
}

CUptiResult cuptiEnableDomain(uint32_t enable,
                                       CUpti_SubscriberHandle subscriber,
                                       CUpti_CallbackDomain domain) {
    if (enable) {
        cb_runtime_domain = domain;
    }

    return CUPTI_SUCCESS;
}

CUptiResult cuptiActivityEnable(CUpti_ActivityKind kind) {
    return CUPTI_SUCCESS;
}

CUptiResult cuptiActivityDisable(CUpti_ActivityKind kind) {
    return CUPTI_SUCCESS;
}

CUptiResult cuptiActivityGetNextRecord(uint8_t* buffer, size_t validBufferSizeBytes,
        CUpti_Activity **record) {

    // First element in buffer tells us which position was returned previously
    CUpti_Activity *meta = (CUpti_Activity*)buffer;
    int64_t read_index = meta->start + 1;

    if (sizeof(CUpti_Activity) * (read_index + 1) > validBufferSizeBytes) {
        *record = NULL;
        return CUPTI_ERROR_MAX_LIMIT_REACHED;
    }

    *record = (CUpti_Activity *)(buffer + read_index * sizeof(CUpti_Activity));
    ++meta->start;

    return CUPTI_SUCCESS;
}

CUptiResult cuptiActivityFlushAll(uint32_t flag) {

    flush_buffer();

    return CUPTI_SUCCESS;
}

CUptiResult cuptiActivityRegisterCallbacks(
        CUpti_BuffersCallbackRequestFunc  funcBufferRequested,
        CUpti_BuffersCallbackCompleteFunc funcBufferCompleted) {

    cb_buffer_request_fn = funcBufferRequested;
    cb_buffer_completed_fn = funcBufferCompleted;

    return CUPTI_SUCCESS;
}

CUptiResult cuptiGetResultString(CUptiResult result, const char **str) {
    *str = "CUPTI stub";
    return CUPTI_SUCCESS;
}

// functions called from the test to trigger GPU events

void cupti_stub_call_runtime(void) {

    if (cb_runtime_fn != NULL) {

        cb_runtime_fn(cb_runtime_userdata, cb_runtime_domain, 1,
                &(const CUpti_CallbackData){
                        .callbackSite = CUPTI_API_ENTER,
                        .functionName = "Test CUDA Runtime call",
                        });

        cb_runtime_fn(cb_runtime_userdata, cb_runtime_domain, 1,
                &(const CUpti_CallbackData){
                        .callbackSite = CUPTI_API_EXIT,
                        .functionName = "Test CUDA Runtime call",
                        });
    }
}

void cupti_stub_call_kernel(void) {

    uint64_t timestamp = get_time();
    CUpti_Activity record = {
        .kind = CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL,
        .start = timestamp,
        .end = timestamp + 1,
    };

    add_record(&record);
}

void cupti_stub_call_memory_op(void) {

    uint64_t timestamp = get_time();
    CUpti_Activity record = {
        .kind = CUPTI_ACTIVITY_KIND_MEMCPY,
        .start = timestamp,
        .end = timestamp + 1,
    };

    add_record(&record);
}
