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

#include "talp/talp_hwc.h"

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


typedef struct {
    bool active;
    bool have_last;
    hw_counters_t last_raw;    // last raw value from backend
} hwc_ctx_t;

static __thread hwc_ctx_t ctx = {0};

static const backend_api_t *hwc_backend_api = NULL;


int talp_hwc_init(const subprocess_descriptor_t *spd) {

    hwc_backend_api = talp_backend_manager_load_hwc_backend(NULL);
    if (hwc_backend_api == NULL) {
        debug_warning("HWC backend could not be loaded");
        return DLB_ERR_UNKNOWN;
    }

    int error;

    /* If HWC component is not explicitly set, probe plugin first */
    if (!(spd->options.talp & TALP_COMPONENT_HWC)) {
        error = hwc_backend_api->probe();
        if (error == DLB_BACKEND_ERROR) {
            debug_warning("HWC backend probe failed");
            return DLB_ERR_UNKNOWN;
        }
    }

    error = hwc_backend_api->init(&core_api);
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("HWC backend could not be initialized");
        return DLB_ERR_UNKNOWN;
    }

    error = hwc_backend_api->start();
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("HWC backend could not be started");
        hwc_backend_api->finalize();
        return DLB_ERR_UNKNOWN;
    }

    return DLB_SUCCESS;
}


int talp_hwc_thread_init(void) {

    if (hwc_backend_api == NULL) return DLB_ERR_UNKNOWN;

    int error = hwc_backend_api->start();
    if (error == DLB_BACKEND_ERROR) return DLB_ERR_UNKNOWN;

    return DLB_SUCCESS;
}


void talp_hwc_finalize(void) {

    if (hwc_backend_api != NULL) {

        hwc_backend_api->stop();
        hwc_backend_api->finalize();

        talp_backend_manager_unload_hwc_backend();
        hwc_backend_api = NULL;
    }
}

void talp_hwc_thread_finalize(void) {

    if (hwc_backend_api != NULL) {
        hwc_backend_api->stop();
    }
}

void talp_hwc_on_state_change(talp_sample_state_t old_state, talp_sample_state_t new_state) {

    if (old_state != TALP_STATE_USEFUL
            && new_state == TALP_STATE_USEFUL) {

        ctx.active = true;
        if (hwc_backend_api->hwc.collect(&ctx.last_raw) == DLB_BACKEND_SUCCESS) {
            ctx.have_last = true;
        }
    }

    else if (old_state == TALP_STATE_USEFUL
            && new_state != TALP_STATE_USEFUL) {
        ctx.active = false;
    }
}

bool talp_hwc_collect(hw_counters_t *out) {

    if (!ctx.active) {
        return false;
    }

    hw_counters_t raw;
    if (hwc_backend_api->hwc.collect(&raw) != DLB_BACKEND_SUCCESS) {
        return false;
    }

    /* Only if we try to collect before changing to 'useful' for the first time,
     * discard this reading and use it as baseline for the next. */
    if (unlikely(!ctx.have_last)) {
        ctx.have_last = true;
        ctx.last_raw = raw;
        return false;
    }

    *out = (hw_counters_t) {
        .cycles       = raw.cycles       - ctx.last_raw.cycles,
        .instructions = raw.instructions - ctx.last_raw.instructions,
    };

    ctx.last_raw = raw;

    return true;
}
