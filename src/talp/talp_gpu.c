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
#include "support/atomic.h"
#include "support/debug.h"
#include "support/dlb_common.h"
#include "talp/backend.h"
#include "talp/backend_manager.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"


static const backend_api_t *gpu_backend_api = NULL;
static gpu_measurements_t gpu_sample = {0};

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

    return DLB_SUCCESS;
}


// Called from talp core
void talp_gpu_finalize(void) {

    if (gpu_backend_api != NULL) {

        gpu_backend_api->stop();
        gpu_backend_api->flush();
        gpu_backend_api->finalize();

        talp_backend_manager_unload_gpu_backend();
        gpu_backend_api = NULL;
    }
}


// Called from GPU backend plugin: CPU enters GPU runtime
void talp_gpu_enter_runtime(void) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(spd, sample, TALP_NO_TIMESTAMP);
        // or: talp_flush_samples_to_regions(spd);

        /* Into Sync call -> not_useful_gpu */
        talp_set_sample_state(spd, sample, TALP_STATE_NOT_USEFUL_GPU);
    }
}


// Called from GPU backend plugin: CPU exits GPU runtime
void talp_gpu_exit_runtime(void) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_gpu_runtime_calls, 1);
        talp_update_sample(spd, sample, TALP_NO_TIMESTAMP);
        // or: talp_flush_samples_to_regions(spd);

        /* Out of Sync call -> useful */
        talp_set_sample_state(spd, sample, TALP_STATE_USEFUL);
    }
}


// Called from GPU backend plugin: flush GPU data
void talp_gpu_submit(const gpu_measurements_t *measurements) {

    gpu_sample.useful_time        += measurements->useful_time;
    gpu_sample.communication_time += measurements->communication_time;
    gpu_sample.inactive_time      += measurements->inactive_time;
}

// called from core
void talp_gpu_collect(gpu_measurements_t *out) {

    // flush GPU, may cause callback to talp_gpu_submit
    gpu_backend_api->flush();

    *out = gpu_sample;
    gpu_sample = (const gpu_measurements_t){0};
}
