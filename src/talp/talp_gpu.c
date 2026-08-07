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

#include "talp/talp_gpu.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_core/thread_ctx.h"
#include "support/debug.h"
#include "support/dlb_common.h"
#include "support/gpu_mask_utils.h"
#include "talp/backend.h"
#include "talp/backend_manager.h"
#include "talp/regions.h"
#include "talp/sample.h"
#include "talp/talp.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"

#include <string.h>


static const backend_api_t *gpu_backend_api = NULL;
static gpu_device_entry_t *devices = NULL;
static size_t num_devices = 0;

// Called from talp core
int talp_gpu_init(const subprocess_descriptor_t *spd) {

    gpu_backend_api = talp_backend_manager_load_gpu_backend(spd->options.talp_gpu_backend);
    if (gpu_backend_api == NULL) {
        debug_warning("GPU backend could not be loaded");
        return DLB_ERR_UNKNOWN;
    }

    int error;

    /* If GPU component is not explicitly set, probe plugin first */
    if (!(spd->options.talp & TALP_COMPONENT_GPU)) {
        error = gpu_backend_api->probe();
        if (error == DLB_BACKEND_ERROR) {
            debug_warning("HWC backend probe failed");
            return DLB_ERR_UNKNOWN;
        }
    }

    error = gpu_backend_api->init(&core_api);
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("GPU backend could not be initialized");
        return DLB_ERR_UNKNOWN;
    }

    error = gpu_backend_api->start();
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("GPU backend could not be started");
        gpu_backend_api->finalize();
        return DLB_ERR_UNKNOWN;
    }

    if (gpu_backend_api->capabilities.gpu_amd) {
        talp_output_record_gpu_vendor(GPU_VENDOR_AMD);
    } else if (gpu_backend_api->capabilities.gpu_nvidia) {
        talp_output_record_gpu_vendor(GPU_VENDOR_NVIDIA);
    }

    error = gpu_backend_api->gpu.get_devices(NULL, 0, &num_devices);
    if (error == DLB_BACKEND_ERROR || num_devices == 0) {
        debug_warning("GPU backend could not obtain number of devices");
        gpu_backend_api->stop();
        gpu_backend_api->finalize();
    }

    devices = malloc(sizeof(*devices) * num_devices);
    error = gpu_backend_api->gpu.get_devices(devices, num_devices, NULL);
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("GPU backend could not obtain device unique IDs");
        gpu_backend_api->stop();
        gpu_backend_api->finalize();
    }

    if (num_devices > MAX_NODE_GPUS) {
        warning("%zu devices registered within a node but MAX_NODE_GPUS=%d. "
                "If you think this is an error, please report bug.",
                num_devices, MAX_NODE_GPUS);
    }

    return DLB_SUCCESS;
}


// Called from talp core
void talp_gpu_finalize(void) {

    if (gpu_backend_api != NULL) {

        gpu_backend_api->stop();
        gpu_backend_api->finalize();

        free(devices);
        devices = NULL;
        num_devices = 0;

        talp_backend_manager_unload_gpu_backend();
        gpu_backend_api = NULL;
    }
}


uint64_t talp_gpu_local_to_unique_id(uint32_t local_id) {

    for (size_t i = 0; i < num_devices; ++i) {
        if (devices[i].local_id == local_id) {
            return devices[i].node_unique_id;
        }
    }

    return ULLONG_MAX;
}


// Called from GPU backend plugin: CPU enters GPU runtime
void talp_gpu_enter_runtime(void) {

    /* Observer and unknown threads may call GPU offload functions, but TALP must ignore them */
    if (unlikely(!thread_is_profiled())) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_update(talp_info);

        /* Into Sync call -> not_useful_gpu */
        talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_GPU);
    }
}


// Called from GPU backend plugin: CPU exits GPU runtime
void talp_gpu_exit_runtime(void) {

    /* Observer and unknown threads may call GPU offload functions, but TALP must ignore them */
    if (unlikely(!thread_is_profiled())) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_update(talp_info);

        /* Add statistic */
        talp_sample_t *sample = talp_sample_get(talp_info);
        ++sample->stats.num_gpu_runtime_calls;

        /* Out of Sync call -> useful */
        talp_sample_set_state(talp_info, TALP_STATE_USEFUL);

        /* Only when needed, update all regions */
        if (talp_info->flags.external_profiler
                && thread_is_main_sequential()) {
            talp_aggregate_samples_to_regions(talp_info);
        }
    }
}


// Called from core
void talp_gpu_collect(gpu_timers_t *out, size_t capacity, uint64_t *out_mask) {

    ensure(thread_is_main(), "Non-main thread collecting GPU measurements. Please report bug.");

    gpu_backend_api->gpu.collect(out, capacity, out_mask);
}
