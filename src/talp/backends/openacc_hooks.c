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

#include "talp/backends/openacc_hooks.h"
#include "talp/backends/backend_utils.h"
#include "talp/backends/dlb_openacc_abi.h"

#include <dlfcn.h>

/*********************************************************************************/
/*  OpenACC profiling                                                            */
/*********************************************************************************/

static const core_api_t *dlb_core_api = NULL;
static dlb_acc_event_t current_event = dlb_acc_ev_none;

/* OpenACC Profiling for the Host constructs */
static void openacc_callback(dlb_acc_prof_info *info, dlb_acc_event_info *event,
        dlb_acc_api_info *api) {

    if (!info) return;

    (void)event;
    (void)api;

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

    dlb_acc_event_t new_event = info->event_type;

    switch(current_event) {
        case dlb_acc_ev_none:
            switch(new_event) {
                case dlb_acc_ev_enter_data_start:
                    PLUGIN_PRINT(" >> acc_ev_enter_data_start\n");
                    break;
                case dlb_acc_ev_exit_data_start:
                    PLUGIN_PRINT(" >> acc_ev_exit_data_start\n");
                    break;
                case dlb_acc_ev_compute_construct_start:
                    PLUGIN_PRINT(" >> acc_ev_compute_construct_start\n");
                    break;
                default:
                    PLUGIN_ERROR("Found unexpected event %d\n", new_event);
                    return;
            }

            dlb_core_api->gpu.enter_runtime();
            current_event = new_event;
            break;

        case dlb_acc_ev_enter_data_start:
            if (new_event == dlb_acc_ev_enter_data_end) {
                PLUGIN_PRINT(" << acc_ev_enter_data_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = dlb_acc_ev_none;
            } else {
                PLUGIN_ERROR("Found unexpected event %d after acc_ev_enter_data_start\n",
                        new_event);
                return;
            }
            break;

        case dlb_acc_ev_exit_data_start:
            if (new_event == dlb_acc_ev_exit_data_end) {
                PLUGIN_PRINT(" << acc_ev_exit_data_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = dlb_acc_ev_none;
            } else {
                PLUGIN_ERROR("Found unexpected event %d after acc_ev_exit_data_start\n",
                        new_event);
                return;
            }
            break;

        case dlb_acc_ev_compute_construct_start:
            if (new_event == dlb_acc_ev_compute_construct_end) {
                PLUGIN_PRINT(" << acc_ev_compute_construct_end\n");
                dlb_core_api->gpu.exit_runtime();
                current_event = dlb_acc_ev_none;
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

void openacc_hooks_set_core_api(const core_api_t *core_api) {
    dlb_core_api = core_api;
}

void openacc_hooks_try_init(void) {

    typedef void (*fn_acc_prof_register_t)(dlb_acc_event_t,
            dlb_acc_prof_callback, dlb_acc_register_t);
    fn_acc_prof_register_t real_acc_prof_register = NULL;

    void *handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        real_acc_prof_register = (fn_acc_prof_register_t)dlsym(handle, "acc_prof_register");
    }
    if (real_acc_prof_register) {
        real_acc_prof_register(dlb_acc_ev_enter_data_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_register(dlb_acc_ev_enter_data_end, openacc_callback, dlb_acc_reg);
        real_acc_prof_register(dlb_acc_ev_exit_data_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_register(dlb_acc_ev_exit_data_end, openacc_callback, dlb_acc_reg);
        real_acc_prof_register(dlb_acc_ev_compute_construct_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_register(dlb_acc_ev_compute_construct_end, openacc_callback, dlb_acc_reg);
    }
}

void openacc_hooks_try_finalize(void) {

    typedef void (*fn_acc_prof_unregister_t)(dlb_acc_event_t,
            dlb_acc_prof_callback, dlb_acc_register_t);
    fn_acc_prof_unregister_t real_acc_prof_unregister = NULL;

    void *handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        real_acc_prof_unregister = (fn_acc_prof_unregister_t)dlsym(handle, "acc_prof_unregister");
    }
    if (real_acc_prof_unregister) {
        real_acc_prof_unregister(dlb_acc_ev_enter_data_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_unregister(dlb_acc_ev_enter_data_end, openacc_callback, dlb_acc_reg);
        real_acc_prof_unregister(dlb_acc_ev_exit_data_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_unregister(dlb_acc_ev_exit_data_end, openacc_callback, dlb_acc_reg);
        real_acc_prof_unregister(dlb_acc_ev_compute_construct_start, openacc_callback, dlb_acc_reg);
        real_acc_prof_unregister(dlb_acc_ev_compute_construct_end, openacc_callback, dlb_acc_reg);
    }
}
