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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/dlb_common.h"
#include "talp/backend.h"
#include "talp/backends/backend_utils.h"
#include "talp/backends/gpu_record_utils.h"

#include <cuda_runtime.h>
#include <cupti.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static const core_api_t *dlb_core_api = NULL;

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
        PLUGIN_WARNING(
                "OpenACC profiling with multi-threading is not yet fully supported. "
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
                    PLUGIN_ERROR("Found unexpected event %d\n", new_event);
                    return;
            }

            dlb_core_api->gpu.enter_runtime();
            current_event = new_event;
            break;

        case acc_ev_enter_data_start:
            if (new_event == acc_ev_enter_data_end) {
                PLUGIN_PRINT(" << acc_ev_enter_data_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = acc_ev_none;
            } else {
                PLUGIN_ERROR("Found unexpected event %d after acc_ev_enter_data_start\n",
                        new_event);
                return;
            }
            break;

        case acc_ev_exit_data_start:
            if (new_event == acc_ev_exit_data_end) {
                PLUGIN_PRINT(" << acc_ev_exit_data_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = acc_ev_none;
            } else {
                PLUGIN_ERROR("Found unexpected event %d after acc_ev_exit_data_start\n",
                        new_event);
                return;
            }
            break;

        case acc_ev_compute_construct_start:
            if (new_event == acc_ev_compute_construct_end) {
                PLUGIN_PRINT(" << acc_ev_compute_construct_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = acc_ev_none;
            } else {
                PLUGIN_ERROR("Found unexpected event %d after acc_ev_compute_construct_start\n",
                        new_event);
                return;
            }
            break;

        default:
            PLUGIN_ERROR("Found unexpected event %d\n", new_event);
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
            PLUGIN_ERROR(                                                       \
                    "%s:%d: Function %s failed with error (%d): %s.\n",         \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
                                                                                \
            return DLB_BACKEND_ERROR;                                           \
        }                                                                       \
  } while (0)

#define CHECK_WARN_CUPTI(call)                                                  \
    do {                                                                        \
        CUptiResult _status = (call);                                           \
        if (_status != CUPTI_SUCCESS) {                                         \
            const char *_error_string;                                          \
            cuptiGetResultString(_status, &_error_string);                      \
            PLUGIN_WARNING(                                                     \
                    "%s:%d: Function %s failed with error (%d): %s.\n",         \
                    __FILE__, __LINE__, #call, _status, _error_string);         \
        }                                                                       \
    } while (0)


/* --- CUPTI state ------------------------------------------------------------- */

static bool cupti_plugin_initialized = false;
static bool cupti_plugin_started = false;
static CUpti_SubscriberHandle subscriber;


/* --- Host runtime events ----------------------------------------------------- */

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
            dlb_core_api->gpu.enter_runtime();
        }
        if (pCallbackInfo->callbackSite == CUPTI_API_EXIT) {
            PLUGIN_PRINT(" << %s\n", pCallbackInfo->functionName);
            dlb_core_api->gpu.exit_runtime();
        }
    }
}


/* --- CUDA activities to profile ---------------------------------------------- */

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


/* Copy relevant information (e.g., start and end timestamps) into our
 * own kernel and memory buffers */
static void process_buffer_records(uint8_t *buffer, size_t valid_size) {

    CUpti_Activity *record = NULL;
    CUptiResult status = cuptiActivityGetNextRecord(buffer, valid_size, &record);
    while (status == CUPTI_SUCCESS) {
        switch (record->kind) {
            case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL: {

                ACTIVITY_KERNEL_TYPE *kernel_record = (ACTIVITY_KERNEL_TYPE *)record;

                uint64_t kernel_start = kernel_record->start;
                uint64_t kernel_end = kernel_record->end;

                if (kernel_start >= safe_timestamp
                        && kernel_end > safe_timestamp) {

                    gpu_record_append_event(&kernel_buffer, kernel_start, kernel_end);

                    PLUGIN_PRINT("KERNEL: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            kernel_start, kernel_end, kernel_end - kernel_start);
                }

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMORY2: {
                ACTIVITY_MEMORY2_TYPE *memory_record = (ACTIVITY_MEMORY2_TYPE *)record;

                // CUPTI_ACTIVITY_KIND_MEMORY2 operations do not provide a duration
                uint64_t memory_start = memory_record->timestamp;
                uint64_t memory_end = memory_record->timestamp;

                if (memory_start >= safe_timestamp
                        && memory_end > safe_timestamp) {

                    gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                    PLUGIN_PRINT("MEMORY2: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            memory_start, memory_end, memory_end - memory_start);
                }

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMSET: {
                ACTIVITY_MEMSET_TYPE *memory_record = (ACTIVITY_MEMSET_TYPE *)record;

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                if (memory_start >= safe_timestamp
                        && memory_end > safe_timestamp) {

                    gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                    PLUGIN_PRINT("MEMSET: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            memory_start, memory_end, memory_end - memory_start);
                }

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMCPY: {
                ACTIVITY_MEMCPY_TYPE *memory_record = (ACTIVITY_MEMCPY_TYPE *)record;

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                if (memory_start >= safe_timestamp
                        && memory_end > safe_timestamp) {

                    gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                    PLUGIN_PRINT("MEMCPY: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            memory_start, memory_end, memory_end - memory_start);
                }

                break;
            }
            case CUPTI_ACTIVITY_KIND_MEMCPY2: {
                ACTIVITY_MEMCPY2_TYPE *memory_record = (ACTIVITY_MEMCPY2_TYPE *)record;

                uint64_t memory_start = memory_record->start;
                uint64_t memory_end = memory_record->end;

                if (memory_start >= safe_timestamp
                        && memory_end > safe_timestamp) {

                    gpu_record_append_event(&memory_buffer, memory_start, memory_end);

                    PLUGIN_PRINT("MEMCPY2: start=%"PRIu64", end=%"PRIu64", duration=%"PRIu64"\n",
                            memory_start, memory_end, memory_end - memory_start);
                }

                break;
            }
            default:
                PLUGIN_PRINT("UNKNOWN activity kind: %d\n", record->kind);
                break;
        }

        status = cuptiActivityGetNextRecord(buffer, valid_size, &record);
    }
}


/* --- CUPTI buffers management ------------------------------------------------ */

/* These are the buffers that are provided to CUPTI and are returned to us
 * when needed (buffer is full or explicitly flushed).
 * The buffer_pool is allocated at initialization time and is composed
 * of NUM_BUFFERS of BUFFER_SIZE each. (now 4 MB * 16 = 64 MB)
 *
 * anffd */

typedef enum { BUF_FREE = 0, BUF_IN_USE, BUF_READY } buffer_state_t;

typedef struct {
    uint8_t *ptr;
    size_t valid_size;
    buffer_state_t state;
} buffer_entry_t;

enum { NUM_BUFFERS = 16 };
enum { BUFFER_SIZE = 4 * 1024 * 1024 };
static buffer_entry_t buffer_pool[NUM_BUFFERS];
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_buffer_pool(void) {

    enum { ALIGNMENT = 64 };
    void *big_block = NULL;

    /* Allocate a contiguos block of memory */
    if (posix_memalign(&big_block, ALIGNMENT, BUFFER_SIZE * NUM_BUFFERS) != 0) {
        PLUGIN_ERROR("Failed to allocate buffer pool\n");
        exit(1);
    }

    pthread_mutex_lock(&buffer_mutex);

    /* Distribute the big block into NUM_BUFFERS buffers */
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        buffer_pool[i].ptr = (uint8_t*)big_block + i * BUFFER_SIZE;
        buffer_pool[i].valid_size = 0;
        buffer_pool[i].state = BUF_FREE;
    }

    pthread_mutex_unlock(&buffer_mutex);
}

static void finalize_buffer_pool(void) {

    pthread_mutex_lock(&buffer_mutex);

    /* Check state correctness */
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (buffer_pool[i].state != BUF_FREE) {
            PLUGIN_ERROR("Buffer %d in state %s while finalizing buffer pool\n",
                    i, buffer_pool[i].state == BUF_IN_USE ? "IN_USE" : "READY");
            exit(1);
        }
    }

    /* Free and set to 0 */
    free(buffer_pool[0].ptr);
    memset(buffer_pool, 0, sizeof(buffer_entry_t) * NUM_BUFFERS);

    pthread_mutex_unlock(&buffer_mutex);
}

/* Called when sample is finished */
static void process_ready_buffers(void) {

    pthread_mutex_lock(&buffer_mutex);

    for (int i = 0; i < NUM_BUFFERS; i++) {

        if (buffer_pool[i].state == BUF_READY) {
            process_buffer_records(buffer_pool[i].ptr, buffer_pool[i].valid_size);
            buffer_pool[i].state = BUF_FREE;
            buffer_pool[i].valid_size = 0;
        }

    }

    pthread_mutex_unlock(&buffer_mutex);
}


/* --- CUPTI asynchronous callbacks -------------------------------------------- */

/* Provide CUPTI with a buffer from the pool for writing activity records */
static void CUPTIAPI bufferRequested(uint8_t **buffer, size_t *size, size_t *maxNumRecords) {

    pthread_mutex_lock(&buffer_mutex);

    /* Look for a FREE buffer */
    int i;
    for (i = 0; i < NUM_BUFFERS; ++i) {
        if (buffer_pool[i].state == BUF_FREE) {
            buffer_pool[i].state = BUF_IN_USE;
            break;
        }
    }

    if (i == NUM_BUFFERS) {
        /* No free buffer, look for a READY buffer and process it */
        for (i = 0; i < NUM_BUFFERS; ++i) {
            if (buffer_pool[i].state == BUF_READY) {
                process_buffer_records(buffer_pool[i].ptr, buffer_pool[i].valid_size);
                buffer_pool[i].state = BUF_IN_USE;
                buffer_pool[i].valid_size = 0;
                break;
            }
        }
    }

    pthread_mutex_unlock(&buffer_mutex);

    if (i < NUM_BUFFERS) {
        /* Return buffer */
        *buffer = buffer_pool[i].ptr;
        *size = BUFFER_SIZE;
        *maxNumRecords = 0;
    } else {
        /* No available buffers, error */
        PLUGIN_ERROR("All CUPTI buffers in use\n");
        *buffer = NULL;
        *size = 0;
        *maxNumRecords = 0;
    }
}

/* Called by CUPTI when an activity buffer is full or needs to be flushed.
 * The callback may be invoked concurrently by multiple CUPTI worker threads,
 * so we just mark the buffer as READY for later processing to keep the
 * callback fast and avoid blocking CUPTI.*/
static void CUPTIAPI bufferCompleted(
    CUcontext ctx, uint32_t streamId, uint8_t *buffer, size_t size, size_t valid_size)
{
    pthread_mutex_lock(&buffer_mutex);

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (buffer_pool[i].ptr == buffer) {
            if (buffer_pool[i].state != BUF_IN_USE) {
                PLUGIN_ERROR("CUPTI buffer %d is completed with wrong state %s\n",
                        i, buffer_pool[i].state == BUF_FREE ? "FREE" : "READY");
            }

            /* Buffer found, mark as ready */
            buffer_pool[i].state = BUF_READY;
            buffer_pool[i].valid_size = valid_size;

            break;
        }
    }

    pthread_mutex_unlock(&buffer_mutex);
}


/* --- Plugin functions  ------------------------------------------------------- */

/* Function called externally by TALP or when stopping plugin to force flushing buffers */
static void cupti_backend_flush(void) {

    /* Once buffers are processed, we need to update the new safe timestamp but we
     * capture it here to not loose records that happen between flushing and processing */
    uint64_t new_safe_timestamp = get_timestamp();

    /* Flush buffer */
    if (cupti_plugin_started) {
        /* This is a blocking call, it does not return until all activity records
        * are returned using the registered callback. */
        cuptiActivityFlushAll(0);
    }

    if (cupti_plugin_initialized) {

        /* Extract records from ready buffers into our own kernel and memory buffers */
        process_ready_buffers();

        /* Update safe timestamp. All future records prior to this will be ignored. */
        safe_timestamp = new_safe_timestamp;

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

static int cupti_backend_probe(void) {
    return DLB_BACKEND_SUCCESS;
}

static int cupti_backend_init(const core_api_t *core_api) {

    if (cupti_plugin_initialized) return DLB_BACKEND_ERROR;

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION
            || dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    /* Allocate buffer pool for CUPTI records */
    init_buffer_pool();

    /* Allocate local buffers */
    enum { LOCAL_BUFFER_INITIAL_CAPACITY = 256 * 1024 }; // 256k records = 4MB
    gpu_record_init_buffer(&kernel_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);
    gpu_record_init_buffer(&memory_buffer, LOCAL_BUFFER_INITIAL_CAPACITY);

    cupti_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

static int cupti_backend_start(void) {

    if (!cupti_plugin_initialized) return DLB_BACKEND_ERROR;

    if (cupti_plugin_started) return DLB_BACKEND_SUCCESS;

    /* Set initial safe timestamp. All records prior to this will be ignored */
    safe_timestamp = get_timestamp();

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

    cupti_plugin_started = true;

    return DLB_BACKEND_SUCCESS;
}

static int cupti_backend_stop(void) {

    if (!cupti_plugin_started) return DLB_BACKEND_SUCCESS;

    /* Flush now and send measurements to TALP */
    cupti_backend_flush();

    /* Stop measurements */
    CHECK_CUPTI(cuptiUnsubscribe(subscriber));
    for (size_t i = 0; i < sizeof(activity_kinds)/sizeof(activity_kinds[0]); ++i) {
        CHECK_WARN_CUPTI(cuptiActivityDisable(activity_kinds[i]));
    }

    try_finalize_openacc_hooks();

    /* Free internal buffers and CUDA events */
    CHECK_CUPTI(cuptiFinalize());

    cupti_plugin_started = false;

    return DLB_BACKEND_SUCCESS;
}

static int cupti_backend_finalize(void) {

    if (!cupti_plugin_initialized) return DLB_BACKEND_SUCCESS;

    /* Free local buffers */
    gpu_record_free_buffer(&kernel_buffer);
    gpu_record_free_buffer(&memory_buffer);

    /* Free buffer pool */
    finalize_buffer_pool();

    cupti_plugin_initialized = false;

    return DLB_BACKEND_SUCCESS;
}

static int cupti_backend_get_gpu_affinity(char *buffer, size_t buffer_size, bool full_uuid) {

    char *b = buffer;
    size_t remaining = buffer_size;
    int written;

    int device_count;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess) {
        return DLB_BACKEND_ERROR;
    }

    int default_device;
    if (cudaGetDevice(&default_device) != cudaSuccess) {
        return DLB_BACKEND_ERROR;
    }

    struct cudaDeviceProp *prop_visible =
        (struct cudaDeviceProp*)calloc(device_count, sizeof(struct cudaDeviceProp));

    for (int gpu = 0; gpu < device_count; gpu++) {
        if (cudaGetDeviceProperties(&prop_visible[gpu], gpu) != cudaSuccess) {
            return DLB_BACKEND_ERROR;
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

    return DLB_BACKEND_SUCCESS;

trunc:
    if (buffer_size > 0) {
        buffer[buffer_size - 1] = '\0';
    }
    return DLB_BACKEND_ERROR;
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void) {
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "cupti",
        .capabilities = {
            .gpu = true,
            .gpu_nvidia = true,
        },
        .probe = cupti_backend_probe,
        .init = cupti_backend_init,
        .start = cupti_backend_start,
        .stop = cupti_backend_stop,
        .finalize = cupti_backend_finalize,
        .flush = cupti_backend_flush,
        .get_gpu_affinity = cupti_backend_get_gpu_affinity,
    };
    return &api;
}
