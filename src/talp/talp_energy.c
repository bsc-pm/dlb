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


#include "talp/talp_energy.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "support/debug.h"
#include "support/dlb_common.h"
#include "support/mytime.h"
#include "talp/backend.h"
#include "talp/backend_manager.h"

// Minimum interval between real backend queries, to keep readings meaningful 
enum { ENERGY_MIN_COLLECT_INTERVAL_NS = 1000000000 };  // 1 second

typedef struct {
    bool              active;         // whether the energy backend is loaded and active
    bool              have_last;      // whether last_raw holds a real reading yet
    energy_readings_t last_raw;       // last raw cumulative readings (nJ) from backend
    int64_t           last_query_ts;  // timestamp (ns) of the last actual backend query
} energy_ctx_t;

static energy_ctx_t ctx = {0};

static const backend_api_t *energy_backend_api = NULL;

int talp_energy_init(const subprocess_descriptor_t *spd)
{
    energy_backend_api = talp_backend_manager_load_energy_backend(spd->options.talp_energy_backend);
    if (energy_backend_api == NULL) {
        debug_warning("Energy backend could not be loaded");
        return DLB_ERR_UNKNOWN;
    }

    int error;
    energy_readings_t first_collect = {0};

    // If energy component is not explicitly set, probe plugin first 
    if (!(spd->options.talp & TALP_COMPONENT_ENERGY)) {
        error = energy_backend_api->probe();
        if (error == DLB_BACKEND_ERROR) {
            debug_warning("Energy backend probe failed");
            talp_backend_manager_unload_energy_backend();
            energy_backend_api = NULL;
            return DLB_ERR_UNKNOWN;
        }
    }

    error = energy_backend_api->init(&core_api);
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("Energy backend could not be initialized");
        talp_backend_manager_unload_energy_backend();
        energy_backend_api = NULL;
        return DLB_ERR_UNKNOWN;
    }

    error = energy_backend_api->start();
    if (error == DLB_BACKEND_ERROR) {
        debug_warning("Energy backend could not be started");
        energy_backend_api->finalize();
        talp_backend_manager_unload_energy_backend();
        energy_backend_api = NULL;
        return DLB_ERR_UNKNOWN;
    }

    ctx.active = true;
    bool collected = talp_energy_collect(&first_collect, NULL);
    if (!collected) {
        debug_warning("Energy collection initialization failed and will be done at a later time");
    }

    return DLB_SUCCESS;
}

void talp_energy_finalize(void)
{
    if (energy_backend_api != NULL) {
        energy_backend_api->stop();
        energy_backend_api->finalize();

        talp_backend_manager_unload_energy_backend();
        energy_backend_api = NULL;
    }

    ctx.active = false;
    ctx.have_last = false;
    ctx.last_raw = (const energy_readings_t){0};
    ctx.last_query_ts = 0;
}

static int64_t delta_guarded(int64_t raw, int64_t last_raw)
{
    // Guard against counter reset/wrap 
    return raw >= last_raw ? raw - last_raw : 0;
}

bool talp_energy_collect(energy_readings_t *out, int64_t *interval_ns)
{
    if (out == NULL) return false;

    *out = (energy_readings_t){0};
    if (interval_ns != NULL) *interval_ns = 0;

    // No backend loaded or not started yet
    if (!ctx.active || energy_backend_api == NULL) {
        return false;
    }

    // Too soon since the last real query; skip this call entirely
    int64_t now = get_time_in_ns();
    if (ctx.have_last && now - ctx.last_query_ts < ENERGY_MIN_COLLECT_INTERVAL_NS) {
        return false;
    }

    // Collect energy reading
    energy_readings_t raw;
    if (energy_backend_api->energy.collect(&raw) != DLB_BACKEND_SUCCESS) {
        return false;
    }

    // Real interval since the last query, computed before last_query_ts moves
    int64_t elapsed = now - ctx.last_query_ts;
    ctx.last_query_ts = now;

    // First reading only sets the baseline 
    if (unlikely(!ctx.have_last)) {
        ctx.have_last = true;
        ctx.last_raw = raw;
        return false;
    }

    // Same delta treatment for all four fields, regardless of backend
    out->cpu  = delta_guarded(raw.cpu, ctx.last_raw.cpu);
    out->dram = delta_guarded(raw.dram, ctx.last_raw.dram);
    out->node = delta_guarded(raw.node, ctx.last_raw.node);
    out->gpu  = delta_guarded(raw.gpu, ctx.last_raw.gpu);

    // Becomes the baseline for the next call's delta
    ctx.last_raw = raw;

    if (interval_ns != NULL) *interval_ns = elapsed;

    return true;
}
