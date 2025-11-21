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


#include "LB_core/spd.h"
#include "apis/dlb.h"
#include "plugins/plugin_manager.h"
#include "support/debug.h"
#include "support/dlb_common.h"
#include "support/options.h"

#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/* The rocprofiler-sdk runtime looks for the symbol "rocprofiler_configure"
 * during initialization. The plugin may not be loaded yet, so this file
 * provides the symbol, initializes the main library if needed, and forwards
 * the call to the plugin's real implementation. */


typedef struct rocprofiler_tool_configure_result rocprofiler_tool_configure_result_t;
typedef struct rocprofiler_client_id rocprofiler_client_id_t;


static void finalize_dlb(void) {
    DLB_Finalize();
}

DLB_EXPORT_SYMBOL
rocprofiler_tool_configure_result_t* rocprofiler_configure(uint32_t version,
        const char* runtime_version, uint32_t priority,
        rocprofiler_client_id_t* client_id) {

    bool dlb_initialized_through_rocprof = false;
    rocprofiler_tool_configure_result_t *result = NULL;

    spd_enter_dlb(thread_spd);

    /* Do something only if --plugin contans "rocprofiler-sdk" */
    char plugins[MAX_OPTION_LENGTH];
    options_parse_entry("--plugin", plugins);
    if (strstr(plugins, "rocprofiler-sdk") != NULL) {

        // Init DLB
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(process_mask), &process_mask);
        int err = DLB_Init(0, &process_mask, NULL);
        if (err == DLB_SUCCESS) {
            dlb_initialized_through_rocprof = true;
        } else if (err != DLB_ERR_INIT) {
            warning("DLB_Init failed: %s. rocprofiler tool will not be configured.",
                    DLB_Strerror(err));
            return NULL;
        }

        // find real rocprofiler_configure in plugin
        typedef rocprofiler_tool_configure_result_t*
            (*real_configure_fn_t)(uint32_t, const char*, uint32_t, rocprofiler_client_id_t*);

        real_configure_fn_t real_rocprofiler_configure =
            (real_configure_fn_t)plugin_get_symbol_from_plugin(
                    "rocprofiler_configure", "rocprofiler-sdk");

        // call real rocprofiler_configure
        if (real_rocprofiler_configure) {
            result = real_rocprofiler_configure(version, runtime_version, priority, client_id);
        }

        if (dlb_initialized_through_rocprof) {
            if (result == NULL) {
                // finalize DLB if error
                DLB_Finalize();
            } else {
                // or at process termination
                atexit(finalize_dlb);
            }
        }
    }

    return result;
}
