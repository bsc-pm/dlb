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

#include "talp/talp_gpu.h"

#include "LB_core/spd.h"
#include "support/atomic.h"
#include "support/dlb_common.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_types.h"

static trigger_update_func_t trigger_update_fn = NULL;

/* Called when regions are stopped and we need to collect GPU measurements */
void talp_gpu_sync_measurements(void) {
    if (trigger_update_fn) trigger_update_fn();
}

/* The following symbols need to be public so that the GPU plugin can call us */

DLB_EXPORT_SYMBOL
void talp_gpu_init(trigger_update_func_t update_fn) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Set flag */
        talp_info->flags.have_gpu = true;

        /* Set up callback function to read measurements */
        trigger_update_fn = update_fn;

        /* Start global region (no-op if already started) */
        region_start(spd, talp_info->monitor);

        /* Set useful state */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

DLB_EXPORT_SYMBOL
void talp_gpu_finalize(void) {
    talp_gpu_sync_measurements();
}

DLB_EXPORT_SYMBOL
void talp_gpu_into_runtime_api(void) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);
        // or: talp_flush_samples_to_regions(spd);

        /* Into Sync call -> not_useful_gpu */
        talp_set_sample_state(sample, not_useful_gpu, talp_info->flags.papi);
    }
}

DLB_EXPORT_SYMBOL
void talp_gpu_out_of_runtime_api(void) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_gpu_runtime_calls, 1);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);
        // or: talp_flush_samples_to_regions(spd);

        /* Out of Sync call -> useful */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

DLB_EXPORT_SYMBOL
void talp_gpu_update_sample(talp_gpu_measurements_t measurements) {

    spd_enter_dlb(thread_spd);
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_info->gpu_sample.timers.useful        += measurements.useful_time;
        talp_info->gpu_sample.timers.communication += measurements.communication_time;
        talp_info->gpu_sample.timers.inactive      += measurements.inactive_time;
    }
}
