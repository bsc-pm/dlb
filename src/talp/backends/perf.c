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

#include "support/dlb_common.h"
#include "talp/backend.h"
#include "talp/backends/backend_utils.h"
#include "talp/backends/perf_event_syscall.h"

#include <inttypes.h>


static const core_api_t *dlb_core_api = NULL;


// some thread local state needed for perf

static bool perf_plugin_initialized = false;
static __thread bool perf_plugin_started = false;
static __thread talp_perf_group_t default_event_group = { .fds = { -1, -1 } };



static int perf_backend_probe(void) {

    talp_perf_group_t group = { .fds = { -1, -1 } };
    if (talp_perf_syscall_open_default(&group) != 0) {
        return DLB_BACKEND_ERROR;
    }

    talp_perf_syscall_close(&group);

    return DLB_BACKEND_SUCCESS;
}

static int perf_backend_init(const core_api_t *core_api) {

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION
            || dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    perf_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

// called once per thread, open the counter group and start it
static int perf_backend_start(void) {

    if (talp_perf_syscall_open_default(&default_event_group) != 0) {
        PLUGIN_ERROR("perf: failed to open counter group\n");
        return DLB_BACKEND_ERROR;
    }

    if (talp_perf_syscall_enable(&default_event_group) != 0) {
        PLUGIN_ERROR("perf: failed to enable counters\n");
        talp_perf_syscall_close(&default_event_group);
        return DLB_BACKEND_ERROR;
    }

    perf_plugin_started = true;

    return DLB_BACKEND_SUCCESS;
}

static int perf_backend_stop(void) {

    if (!perf_plugin_started) return DLB_BACKEND_ERROR;

    talp_perf_syscall_disable(&default_event_group);
    talp_perf_syscall_close(&default_event_group);

    perf_plugin_started = false;

    return DLB_BACKEND_SUCCESS;
}

static int perf_backend_finalize(void) {

    if (!perf_plugin_initialized) return DLB_BACKEND_ERROR;

    perf_plugin_initialized = false;

    return DLB_BACKEND_SUCCESS;
}

static int perf_backend_collect(hw_counters_t *out) {

    if (!perf_plugin_started) return DLB_BACKEND_ERROR;

    hw_counters_t counters = talp_perf_syscall_read(&default_event_group);
    if (counters.cycles < 0 || counters.instructions < 0) {
        PLUGIN_WARNING("perf: error reading counters\n");
        return DLB_BACKEND_ERROR;
    }

    PLUGIN_PRINT("cycles: %" PRId64 ", instructions: %" PRId64 "\n",
            counters.cycles, counters.instructions);

    *out = counters;

    return DLB_BACKEND_SUCCESS;
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void) {
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "perf",
        .capabilities = {
            .hwc = true,
        },
        .probe = perf_backend_probe,
        .init = perf_backend_init,
        .start = perf_backend_start,
        .stop = perf_backend_stop,
        .finalize = perf_backend_finalize,
        .hwc = {
            .collect = perf_backend_collect,
        },
    };
    return &api;
}
