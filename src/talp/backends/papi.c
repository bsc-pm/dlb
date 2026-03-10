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

#include <papi.h>
#include <pthread.h>


static const core_api_t *dlb_core_api = NULL;


/* --- CUPTI state ------------------------------------------------------------- */

static bool papi_plugin_initialized = false;
static __thread bool papi_plugin_started = false;
static __thread int EventSet = PAPI_NULL;


/* --- Plugin functions  ------------------------------------------------------- */

static int papi_backend_probe(void) {

    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT || retval == PAPI_EINVAL) {
        int loaded_lib_version = PAPI_get_opt(PAPI_LIB_VERSION, NULL);
        if (PAPI_VERSION_MAJOR(PAPI_VER_CURRENT) == PAPI_VERSION_MAJOR(loaded_lib_version)) {
            retval = PAPI_library_init(loaded_lib_version);
        }
    }

    if (retval < 0 || PAPI_is_initialized() == PAPI_NOT_INITED) {
        return DLB_BACKEND_ERROR;
    }

    int event_set = PAPI_NULL;
    int error = PAPI_create_eventset(&event_set);
    if (error != PAPI_OK) {
        PAPI_shutdown();
        return DLB_BACKEND_ERROR;
    }

    int Events[2] = {PAPI_TOT_CYC, PAPI_TOT_INS};
    error = PAPI_add_events(event_set, Events, 2);
    if (error != PAPI_OK) {
        PAPI_shutdown();
        return DLB_BACKEND_ERROR;
    }

    PAPI_cleanup_eventset(event_set);
    PAPI_destroy_eventset(&event_set);
    PAPI_shutdown();

    return DLB_BACKEND_SUCCESS;
}

static int papi_backend_init(const core_api_t *core_api) {

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION
            || dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    /* Library init */
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if(retval != PAPI_VER_CURRENT || retval == PAPI_EINVAL){
        // PAPI init failed because of a version mismatch
        // Maybe we can rectify the situation by cheating a bit.
        // Lets first check if the major version is the same
        int loaded_lib_version = PAPI_get_opt(PAPI_LIB_VERSION, NULL);
        if(PAPI_VERSION_MAJOR(PAPI_VER_CURRENT) == PAPI_VERSION_MAJOR(loaded_lib_version)){
            PLUGIN_WARNING("The PAPI version loaded at runtime differs from the one DLB was"
                    " compiled with. Expected version %d.%d.%d but got %d.%d.%d. Continuing"
                    " on best effort.\n",
                PAPI_VERSION_MAJOR(PAPI_VER_CURRENT),
                PAPI_VERSION_MINOR(PAPI_VER_CURRENT),
                PAPI_VERSION_REVISION(PAPI_VER_CURRENT),
                PAPI_VERSION_MAJOR(loaded_lib_version),
                PAPI_VERSION_MINOR(loaded_lib_version),
                PAPI_VERSION_REVISION(loaded_lib_version));
            // retval here can only be loaded_lib_version or negative. We
            // assume loaded_lib_version and check for other failures below
            retval = PAPI_library_init(loaded_lib_version);
        }
        else{
            // we have a different major version. we fail.
            PLUGIN_ERROR("The PAPI version loaded at runtime differs greatly from the one DLB"
                    " was compiled with. Expected version%d.%d.%d but got %d.%d.%d.\n",
                    PAPI_VERSION_MAJOR(PAPI_VER_CURRENT),
                    PAPI_VERSION_MINOR(PAPI_VER_CURRENT),
                    PAPI_VERSION_REVISION(PAPI_VER_CURRENT),
                    PAPI_VERSION_MAJOR(loaded_lib_version),
                    PAPI_VERSION_MINOR(loaded_lib_version),
                    PAPI_VERSION_REVISION(loaded_lib_version));
            return DLB_BACKEND_ERROR;
        }
    }

    if(retval < 0 || PAPI_is_initialized() == PAPI_NOT_INITED){
        // so we failed, we can maybe get some info why:
        switch(retval) {
            case PAPI_ENOMEM:
                PLUGIN_ERROR("PAPI initialization failed: Insufficient memory to complete"
                        " the operation.\n");
                break;
            case PAPI_ECMP:
                PLUGIN_ERROR("PAPI initialization failed: This component does not support"
                    " the underlying hardware.\n");
                break;
            case PAPI_ESYS:
                PLUGIN_ERROR("PAPI initialization failed: A system or C library call failed"
                        " inside PAPI.\n");
                break;
        }

        return DLB_BACKEND_ERROR;
    }

    /* Activate thread tracing */
    int error = PAPI_thread_init(pthread_self);
    if (error != PAPI_OK) {
        PLUGIN_ERROR("PAPI Error during thread initialization. %d: %s\n",
                error, PAPI_strerror(error));
        return DLB_BACKEND_ERROR;
    }

    papi_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

// called once per thread, create EventSet and start
static int papi_backend_start(void) {

    int error = PAPI_register_thread();
    if (error != PAPI_OK) {
        PLUGIN_ERROR("PAPI Error during thread registration. %d: %s\n",
                error, PAPI_strerror(error));
        return DLB_BACKEND_ERROR;
    }

    /* EventSet creation */
    EventSet = PAPI_NULL;
    error = PAPI_create_eventset(&EventSet);
    if (error != PAPI_OK) {
        PLUGIN_ERROR("PAPI Error during eventset creation. %d: %s\n",
                error, PAPI_strerror(error));
        return DLB_BACKEND_ERROR;
    }

    int Events[2] = {PAPI_TOT_CYC, PAPI_TOT_INS};
    error = PAPI_add_events(EventSet, Events, 2);
    if (error != PAPI_OK) {
        PLUGIN_ERROR("PAPI Error adding events. %d: %s\n",
                error, PAPI_strerror(error));
        return DLB_BACKEND_ERROR;
    }

    /* Start tracing  */
    error = PAPI_start(EventSet);
    if (error != PAPI_OK) {
        PLUGIN_ERROR("PAPI Error during tracing initialization: %d: %s\n",
                error, PAPI_strerror(error));
        return DLB_BACKEND_ERROR;
    }

    papi_plugin_started = true;

    return DLB_BACKEND_SUCCESS;
}

static void papi_backend_flush(void);

static int papi_backend_stop(void) {

    if (!papi_plugin_started) return DLB_BACKEND_ERROR;

    /* Flush now and send measurements to TALP */
    papi_backend_flush();

    int error;

    if (EventSet != PAPI_NULL) {
        // Stop counters (ignore if already stopped)
        error = PAPI_stop(EventSet, NULL);
        if (error != PAPI_OK && error != PAPI_ENOTRUN) {
            PLUGIN_WARNING("PAPI Error stopping counters. %d: %s\n",
                    error, PAPI_strerror(error));
        }

        // Cleanup and destroy the EventSet
        error = PAPI_cleanup_eventset(EventSet);
        if (error != PAPI_OK) {
            PLUGIN_WARNING("PAPI Error cleaning eventset. %d: %s\n",
                    error, PAPI_strerror(error));
        }

        error = PAPI_destroy_eventset(&EventSet);
        if (error != PAPI_OK) {
            PLUGIN_WARNING("PAPI Error destroying eventset. %d: %s\n",
                    error, PAPI_strerror(error));
        }

        EventSet = PAPI_NULL;
    }

    // Unregister this thread from PAPI
    error = PAPI_unregister_thread();
    if (error != PAPI_OK) {
        PLUGIN_WARNING("PAPI Error unregistering thread. %d: %s\n",
                error, PAPI_strerror(error));
    }

    papi_plugin_started = false;

    return DLB_BACKEND_SUCCESS;
}

static int papi_backend_finalize(void) {

    if (!papi_plugin_initialized) return DLB_BACKEND_ERROR;

    PAPI_shutdown();

    papi_plugin_initialized = false;

    return DLB_BACKEND_SUCCESS;
}

/* Function called externally by TALP or when stopping plugin to force flushing buffers */
static void papi_backend_flush(void) {

    PLUGIN_PRINT("papi_backend_flush, started? %d\n", papi_plugin_started);

    if (!papi_plugin_started) return;

    long long papi_values[2];
    int error = PAPI_read(EventSet, papi_values);
    if (error != PAPI_OK) {
        PLUGIN_WARNING("Error reading PAPI counters: %d, %s\n", error, PAPI_strerror(error));
        return;
    }

    PLUGIN_PRINT("cycles: %lld, instructions: %lld\n", papi_values[0], papi_values[1]);

    /* Pack values to send to TALP */
    hwc_measurements_t measurements = {
        .cycles = papi_values[0],
        .instructions = papi_values[1],
    };

    /* Call TALP */
    dlb_core_api->hwc.submit_measurements(&measurements);
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void) {
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "papi",
        .capabilities = {
            .hwc = true,
        },
        .probe = papi_backend_probe,
        .init = papi_backend_init,
        .start = papi_backend_start,
        .stop = papi_backend_stop,
        .finalize = papi_backend_finalize,
        .flush = papi_backend_flush,
    };
    return &api;
}
