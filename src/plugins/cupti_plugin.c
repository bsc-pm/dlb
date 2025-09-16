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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "plugins/plugin.h"
#include "plugins/plugin_verbose.h"
#include "plugins/gpu_record_utils.h"
#include "support/dlb_common.h"
#include "talp/talp_gpu.h"
#include <cuda_runtime.h>
#include <cupti.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



/*********************************************************************************/
/*  OpenACC profiling                                                            */
/*********************************************************************************/
#ifdef HAVE_ACC_PROF_H
#include <acc_prof.h>

static bool warned_about_acc_multithread = false;
static acc_event_t current_event = acc_ev_none;

/* OpenACC Profiling for the Host constructs */
static void openacc_callback(acc_prof_info *info, acc_event_info *event, acc_api_info *api) {
    if (!info) return;

    if (!warned_about_acc_multithread && info->thread_id > 1) {
        warned_about_acc_multithread = true;
        fprintf(stderr,
                "Warning: OpenACC profiling with multi-threading is not yet fully supported. "
                "Profiling will continue, but results may be inaccurate.\n");

    }

    /* OpenACC events may be nested, so we will only count the outermost events
     *
     * Observations:
     * - acc_ev_enter_data_{start,end} covers the duration of data operations for a construct.
     *      It typically includes memory creation, deletion, allocation, freeing, uploading,
     *      and downloading.
     *
     * - acc_ev_compute_construct_{start,end} covers the duration of the compute block.
     *      It generally includes kernel launches and queue waits.
     *
     * - acc_ev_exit_data_{start,end} covers final data operations for the completion of a construct.
     */

    acc_event_t new_event = info->event_type;

    switch(current_event) {
        case acc_ev_none:
            switch(new_event) {
                case acc_ev_enter_data_start:
                    PLUGIN_PRINT(" >> acc_ev_enter_data_start\n");
                    break;
                case acc_ev_exit_data_start:
                    PLUGIN_PRINT(" >> acc_ev_exit_data_start\n");
                    break;
                case acc_ev_compute_construct_start:
                    PLUGIN_PRINT(" >> acc_ev_compute_construct_start\n");
                    break;
                default:
                    fprintf(stderr, "Error: Found unexpected event %d\n", new_event);
                    return;
            }

            talp_gpu_into_runtime_api();
            current_event = new_event;
            break;

        case acc_ev_enter_data_start:
            if (new_event == acc_ev_enter_data_end) {
                PLUGIN_PRINT(" << acc_ev_enter_data_end\n");
                talp_gpu_out_of_runtime_api();
                current_event = acc_ev_none;
            } else {
                fprintf(stderr, "Error: Found unexpected event %d after acc_ev_enter_data_start\n",
                        new_event);
                return;
            }
            break;

        case acc_ev_exit_data_start:
            if (new_event == acc_ev_exit_data_end) {
                PLUGIN_PRINT(" << acc_ev_exit_data_end\n");
                talp_gpu_out_of_runtime_api();
                current_event = acc_ev_none;
            } else {
                fprintf(stderr, "Error: Found unexpected event %d after acc_ev_exit_data_start\n",
                        new_event);
                return;
            }
            break;

        case acc_ev_compute_construct_start:
            if (new_event == acc_ev_compute_construct_end) {
                PLUGIN_PRINT(" << acc_ev_compute_construct_end\n");
                talp_gpu_out_of_runtime_api();
                current_event = acc_ev_none;
            } else {
                fprintf(stderr, "Error: Found unexpected event %d after"
                            " acc_ev_compute_construct_start\n",
                        new_event);
                return;
            }
            break;

        default:
            fprintf(stderr, "Error: Found unexpected event %d\n", new_event);
            return;

    }
}

static void try_init_openacc_hooks(void) {

    void (*real_acc_prof_register)(acc_event_t, acc_prof_callback, acc_register_t) = NULL;
    void *handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        real_acc_prof_register = dlsym(handle, "acc_prof_register");
    }
    if (real_acc_prof_register) {
        acc_prof_register(acc_ev_enter_data_start, openacc_callback, acc_reg);
        acc_prof_register(acc_ev_enter_data_end, openacc_callback, acc_reg);
        acc_prof_register(acc_ev_exit_data_start, openacc_callback, acc_reg);
        acc_prof_register(acc_ev_exit_data_end, openacc_callback, acc_reg);
        acc_prof_register(acc_ev_compute_construct_start, openacc_callback, acc_reg);
        acc_prof_register(acc_ev_compute_construct_end, openacc_callback, acc_reg);
    }
}

static void try_finalize_openacc_hooks(void) {

    void (*real_acc_prof_unregister)(acc_event_t, acc_prof_callback, acc_register_t) = NULL;
    void *handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        real_acc_prof_unregister = dlsym(handle, "acc_prof_unregister");
    }
    if (real_acc_prof_unregister) {
        acc_prof_unregister(acc_ev_enter_data_start, openacc_callback, acc_reg);
        acc_prof_unregister(acc_ev_enter_data_end, openacc_callback, acc_reg);
        acc_prof_unregister(acc_ev_exit_data_start, openacc_callback, acc_reg);
        acc_prof_unregister(acc_ev_exit_data_end, openacc_callback, acc_reg);
        acc_prof_unregister(acc_ev_compute_construct_start, openacc_callback, acc_reg);
        acc_prof_unregister(acc_ev_compute_construct_end, openacc_callback, acc_reg);
    }
}

#else

static void try_init_openacc_hooks(void) {}
static void try_finalize_openacc_hooks(void) {}

#endif /* HAVE_ACC_PROF_H */


/*********************************************************************************/
/*  CUDA profiling                                                               */
/*********************************************************************************/


#define CHECK_CUPTI(call)                                                       \
    do {                                                                        \
        CUptiResult _status = (call);                                           \
        if (_status != CUPTI_SUCCESS) {                                         \
            const char *_error_string;                                          \
            cuptiGetResultString(_status, &_error_string);                      \
            fprintf(stderr,                                                     \
                    "%s:%d: Error: Function %s failed with error (%d): %s.\n",  \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
                                                                                \
            return DLB_PLUGIN_ERROR;                                            \
        }                                                                       \
  } while (0)

#define CHECK_WARN_CUPTI(call)                                                  \
    do {                                                                        \
        CUptiResult _status = (call);                                           \
        if (_status != CUPTI_SUCCESS) {                                         \
            const char *_error_string;                                          \
            cuptiGetResultString(_status, &_error_string);                      \
            fprintf(stderr,                                                     \
                    "%s:%d: Warning: Function %s failed with error (%d): %s.\n",\
                    __FILE__, __LINE__, #call, _status, _error_string);         \
        }                                                                       \
    } while (0)


#define CUPTI_BUFFER_SIZE (2 * 1024 * 1024)     // 2MB
#define INTERMEDIATE_BUFFER_SIZE (16 * 1024)    // * 16 bytes/element =  ~256 KB


#if CUPTI_API_VERSION < 18
#  error "CUDA Toolkit 11.8 minimum required"
#endif

#if CUPTI_API_VERSION >= 130000     /* 13.0 */
typedef CUpti_ActivityKernel10 ACTIVITY_KERNEL_TYPE;
#elif CUPTI_API_VERSION >= 19       /* 12.0 */
typedef CUpti_ActivityKernel9 ACTIVITY_KERNEL_TYPE;
#else
typedef CUpti_ActivityKernel8 ACTIVITY_KERNEL_TYPE;
#endif

#if CUPTI_API_VERSION >= 24         /* 12.6 */
typedef CUpti_ActivityMemory4 ACTIVITY_MEMORY2_TYPE;
#else
typedef CUpti_ActivityMemory3 ACTIVITY_MEMORY2_TYPE;
#endif

#if CUPTI_API_VERSION >= 26         /* 12.8 */
typedef CUpti_ActivityMemcpy6 ACTIVITY_MEMCPY_TYPE;
#else
typedef CUpti_ActivityMemcpy5 ACTIVITY_MEMCPY_TYPE;
#endif

typedef CUpti_ActivityMemset4 ACTIVITY_MEMSET_TYPE;

typedef CUpti_ActivityMemcpyPtoP4 ACTIVITY_MEMCPY2_TYPE;


static bool cupti_plugin_enabled = false;
static atomic_uint_least64_t computation_time = 0;
static atomic_uint_least64_t communication_time = 0;
static CUpti_SubscriberHandle subscriber;

static CUpti_ActivityKind activity_kinds[] = {
    CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL,
    // reports separate records for memory allocation and release operations, but no duration
    CUPTI_ACTIVITY_KIND_MEMORY2,
    // H2H, H2D, D2H, D2D memory copies / sets:
    CUPTI_ACTIVITY_KIND_MEMSET,
    CUPTI_ACTIVITY_KIND_MEMCPY,
    // Peer to peer memory copies:
    CUPTI_ACTIVITY_KIND_MEMCPY2,

    // disabled, reports lifetime of allocations
    /* CUPTI_ACTIVITY_KIND_MEMORY, */
    // to discuss:
    /* CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER, */
    /* CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_TRACKING, */
    /* CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO, */
};

/* CUDA API Runtime calls */
static void CUPTIAPI GetEventValueCallback(
    void *pUserData,
    CUpti_CallbackDomain domain,
    CUpti_CallbackId callbackId,
    const CUpti_CallbackData *pCallbackInfo)
{
    if (domain == CUPTI_CB_DOMAIN_RUNTIME_API) {
        if (pCallbackInfo->callbackSite == CUPTI_API_ENTER) {
            PLUGIN_PRINT(" >> %s\n", pCallbackInfo->functionName);
            talp_gpu_into_runtime_api();
        }
        if (pCallbackInfo->callbackSite == CUPTI_API_EXIT) {
            PLUGIN_PRINT(" << %s\n", pCallbackInfo->functionName);
            talp_gpu_out_of_runtime_api();
        }
    }
}


static void CUPTIAPI bufferRequested(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
{
    // TODO: pre-allocate a pool of buffers and provide them to CUPTI

    void *raw_buffer = NULL;

    // Allocate aligned memory
    enum { ALIGNMENT = 64 };
    if (posix_memalign(&raw_buffer, ALIGNMENT, CUPTI_BUFFER_SIZE) != 0) {
        fprintf(stderr, "Failed to allocate aligned buffer\n");
        *buffer = NULL;
        *size = 0;
        return;
    }

    *buffer = (uint8_t*)raw_buffer;
    *size = CUPTI_BUFFER_SIZE;
    *maxNumRecords = 0;
}

static void CUPTIAPI bufferCompleted(
    CUcontext ctx, uint32_t streamId, uint8_t *buffer, size_t size, size_t validSize)
{
    /* Intermediate buffers for computing time */
    int num_kernel_records = 0;
    gpu_record_t kernel_records_buffer[INTERMEDIATE_BUFFER_SIZE];

    int num_memory_records = 0;
    gpu_record_t memory_records_buffer[INTERMEDIATE_BUFFER_SIZE];

    CUpti_Activity *record = NULL;
    CUptiResult status = cuptiActivityGetNextRecord(buffer, validSize, &record);
    while (status == CUPTI_SUCCESS) {
        switch (record->kind) {
            case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL: {

                if (num_kernel_records >= INTERMEDIATE_BUFFER_SIZE) {
                    fprintf(stderr, "INTERMEDIATE_BUFFER_SIZE reached");
                    break;
                }

                ACTIVITY_KERNEL_TYPE *kernel_record = (ACTIVITY_KERNEL_TYPE *)record;

                uint64_t kernel_start = kernel_record->start;
                uint64_t kernel_end = kernel_record->end;

                kernel_records_buffer[num_kernel_records++] = (gpu_record_t){
                    .start = kernel_start,
                    .end = kernel_end,
                };

                PLUGIN_PRINT("KERNEL: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        kernel_start, kernel_end, kernel_end - kernel_start);

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMORY2: {
                ACTIVITY_MEMORY2_TYPE *memory_record = (ACTIVITY_MEMORY2_TYPE *)record;

                if (num_memory_records >= INTERMEDIATE_BUFFER_SIZE) {
                    fprintf(stderr, "INTERMEDIATE_BUFFER_SIZE reached");
                    break;
                }

                // CUPTI_ACTIVITY_KIND_MEMORY2 operations do not provide a duration
                uint64_t memory_start = memory_record->timestamp;
                uint64_t memory_end = memory_record->timestamp;

                memory_records_buffer[num_memory_records++] = (gpu_record_t){
                    .start = memory_start,
                    .end = memory_end,
                };

                PLUGIN_PRINT("MEMORY2: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        memory_start, memory_end, memory_end - memory_start);

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMSET: {
                ACTIVITY_MEMSET_TYPE *memory_record = (ACTIVITY_MEMSET_TYPE *)record;

                if (num_memory_records >= INTERMEDIATE_BUFFER_SIZE) {
                    fprintf(stderr, "INTERMEDIATE_BUFFER_SIZE reached");
                    break;
                }

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                memory_records_buffer[num_memory_records++] = (gpu_record_t){
                    .start = memory_start,
                    .end = memory_end,
                };

                PLUGIN_PRINT("MEMSET: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        memory_start, memory_end, memory_end - memory_start);

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMCPY: {
                ACTIVITY_MEMCPY_TYPE *memory_record = (ACTIVITY_MEMCPY_TYPE *)record;

                if (num_memory_records >= INTERMEDIATE_BUFFER_SIZE) {
                    fprintf(stderr, "INTERMEDIATE_BUFFER_SIZE reached");
                    break;
                }

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                memory_records_buffer[num_memory_records++] = (gpu_record_t){
                    .start = memory_start,
                    .end = memory_end,
                };

                PLUGIN_PRINT("MEMCPY: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        memory_start, memory_end, memory_end - memory_start);

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMCPY2: {
                ACTIVITY_MEMCPY2_TYPE *memory_record = (ACTIVITY_MEMCPY2_TYPE *)record;

                if (num_memory_records >= INTERMEDIATE_BUFFER_SIZE) {
                    fprintf(stderr, "INTERMEDIATE_BUFFER_SIZE reached");
                    break;
                }

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                memory_records_buffer[num_memory_records++] = (gpu_record_t){
                    .start = memory_start,
                    .end = memory_end,
                };

                PLUGIN_PRINT("MEMCPY2: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                        memory_start, memory_end, memory_end - memory_start);

                break;
            }
            default:
                PLUGIN_PRINT("UNKNOWN activity kind: %d\n", record->kind);
                break;
        }

        status = cuptiActivityGetNextRecord(buffer, validSize, &record);
    }

    free(buffer);

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

/* Function called externally by TALP to force flushing buffers */
static void cupti_plugin_update_sample(void) {

    /* Flush buffer */
    if (cupti_plugin_enabled) {
        /* This is a blocking call, it does not return until all activity records
        * are returned using the registered callback. */
        cuptiActivityFlushAll(0);
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

static int cupti_plugin_init(plugin_info_t *info) {

    /* Subscribe */
    CHECK_CUPTI(cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)GetEventValueCallback, NULL));

    /* Enable RUNTIME API */
    CHECK_CUPTI(cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));

    /* Enable Activity kinds for Kernel and Memory Tracing */
    for (size_t i = 0; i < sizeof(activity_kinds)/sizeof(activity_kinds[0]); ++i) {
        CHECK_WARN_CUPTI(cuptiActivityEnable(activity_kinds[i]));
    }

    CHECK_CUPTI(cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted));

    try_init_openacc_hooks();

    /* Enable GPU component in TALP */
    talp_gpu_init(cupti_plugin_update_sample);

    /* Return some useful information */
    info->name = "cupti";
    info->version = 1;

    cupti_plugin_enabled = true;

    return DLB_PLUGIN_SUCCESS;
}

static int cupti_plugin_finalize(void) {

    if (cupti_plugin_enabled) {
        /* Finalize TALP GPU first to compute unflushed records */
        talp_gpu_finalize();

        CHECK_CUPTI(cuptiUnsubscribe(subscriber));
        for (size_t i = 0; i < sizeof(activity_kinds)/sizeof(activity_kinds[0]); ++i) {
            CHECK_WARN_CUPTI(cuptiActivityDisable(activity_kinds[i]));
        }

        try_finalize_openacc_hooks();

        /* Explicitly flush and free internal buffers and CUDA events
           before MPI_Finalize / library unload */
        CHECK_CUPTI(cuptiFinalize());

        cupti_plugin_enabled = false;
    }

    return DLB_PLUGIN_SUCCESS;
}

static int cupti_plugin_get_affinity(char *buffer, size_t buffer_size, bool full_uuid) {

    char *b = buffer;
    size_t remaining = buffer_size;
    int written;

    int device_count;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess) {
        return DLB_PLUGIN_ERROR;
    }

    int default_device;
    if (cudaGetDevice(&default_device) != cudaSuccess) {
        return DLB_PLUGIN_ERROR;
    }

    struct cudaDeviceProp *prop_visible =
        (struct cudaDeviceProp*)calloc(device_count, sizeof(struct cudaDeviceProp));

    for (int gpu = 0; gpu < device_count; gpu++) {
        if (cudaGetDeviceProperties(&prop_visible[gpu], gpu) != cudaSuccess) {
            return DLB_PLUGIN_ERROR;
        }

        /* GPU id */
        written = snprintf(b, remaining, "%d (UUID=", gpu);
        if (written < 0 || (size_t)written >= remaining) goto trunc;
        b += written; remaining -= written;

        /* UUID */
        CUuuid *u = &prop_visible[gpu].uuid;
        if (full_uuid) {
            written = snprintf(b, remaining,
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    (unsigned char)u->bytes[0],  (unsigned char)u->bytes[1],
                    (unsigned char)u->bytes[2],  (unsigned char)u->bytes[3],
                    (unsigned char)u->bytes[4],  (unsigned char)u->bytes[5],
                    (unsigned char)u->bytes[6],  (unsigned char)u->bytes[7],
                    (unsigned char)u->bytes[8],  (unsigned char)u->bytes[9],
                    (unsigned char)u->bytes[10], (unsigned char)u->bytes[11],
                    (unsigned char)u->bytes[12], (unsigned char)u->bytes[13],
                    (unsigned char)u->bytes[14], (unsigned char)u->bytes[15]);
        } else {
            written = snprintf(b, remaining, "..%02x%02x",
                    (unsigned char)u->bytes[14], (unsigned char)u->bytes[15]);
        }
        if (written < 0 || (size_t)written >= remaining) goto trunc;
        b += written;
        remaining -= written;

        /* default device */
        if (gpu == default_device) {
            written = snprintf(b, remaining, ", default");
            if (written < 0 || (size_t)written >= remaining) goto trunc;
            b += written; remaining -= written;
        }

        if (gpu + 1 != device_count) {
            written = snprintf(b, remaining, "), ");
        } else {
            written = snprintf(b, remaining, ")");
        }
        if (written < 0 || (size_t)written >= remaining) goto trunc;
        b += written; remaining -= written;
    }

    return DLB_PLUGIN_SUCCESS;

trunc:
    if (buffer_size > 0) {
        buffer[buffer_size - 1] = '\0';
    }
    return DLB_PLUGIN_ERROR;
}

DLB_EXPORT_SYMBOL
plugin_api_t* DLB_Get_Plugin_API(void) {
    static plugin_api_t api = {
        .init = cupti_plugin_init,
        .finalize = cupti_plugin_finalize,
        .get_affinity = cupti_plugin_get_affinity,
    };
    return &api;
}
