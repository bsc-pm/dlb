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

#include "talp/backend_manager.h"

#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "talp/backend.h"
#include "talp/talp_gpu.h"
#include "talp/talp_hwc.h"

#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>


// Internal Core interface that plugins may use
const core_api_t core_api = {
    .abi_version = DLB_BACKEND_ABI_VERSION,
    .struct_size = sizeof(core_api_t),
    .gpu = {
        .enter_runtime       = talp_gpu_enter_runtime,
        .exit_runtime        = talp_gpu_exit_runtime,
        .submit_measurements = talp_gpu_submit,
    },
    .hwc = {
        .submit_measurements = talp_hwc_submit,
    },
};

// Loaded plugins. We only accept one plugin per capability
typedef struct plugin_t {
    const backend_api_t *api;
    void                *handle;
} plugin_t;


/* --- GPU plguins  ------------------------------------------------------------ */

// List of possible GPU backend plugin names
static const char *gpu_plugins[] = {
    "cupti",
    "rocprofiler-sdk",
    "rocprofilerv2",
};

enum { num_gpu_plugins = sizeof(gpu_plugins) / sizeof(gpu_plugins[0]) };

static plugin_t loaded_gpu_plugin = {};


/* --- HWC plguins  ------------------------------------------------------------ */

// List of possible HWC backend plugin names
static const char *hwc_plugins[] = {
    "papi",
};

enum { num_hwc_plugins = sizeof(hwc_plugins) / sizeof(hwc_plugins[0]) };

static plugin_t loaded_hwc_plugin = {};


/* ----------------------------------------------------------------------------- */

/* Obtain full path to libdlb.so (or whatever variant is loaded) */
static const char* get_dlb_lib_path(void) {

    static char dlb_lib_path[PATH_MAX] = "";

    if (dlb_lib_path[0] != '\0') return dlb_lib_path;

    // Get information of the dlb shared object currently loaded
    Dl_info info;
    if (dladdr((void*)__func__, &info)) {
        snprintf(dlb_lib_path, sizeof(dlb_lib_path), "%s", dirname((char*)info.dli_fname));
    } else {
        debug_warning("dladdr failed to obtain information");
        return NULL;
    }

    return dlb_lib_path;
}

/* Obtain DLB_LIB_DIR environment variable */
static const char* get_dlb_plugin_path(void) {

    static char dlb_plugin_path[PATH_MAX] = "";

    if (dlb_plugin_path[0] != '\0') return dlb_plugin_path;

    // Try env var first
    const char *dlb_plugin_path_env = getenv("DLB_LIB_DIR");
    if (dlb_plugin_path_env != NULL) {
        DIR* dir = opendir(dlb_plugin_path_env);
        if (dir) {
            snprintf(dlb_plugin_path, sizeof(dlb_plugin_path), "%s", dlb_plugin_path_env);
            closedir(dir);
        } else {
            dlb_plugin_path_env = NULL;
        }
    }

    // Try same directoy as dlb library
    if (dlb_plugin_path_env == NULL) {
        snprintf(dlb_plugin_path, sizeof(dlb_plugin_path), "%s", get_dlb_lib_path());
    }

    return dlb_plugin_path;
}

/* Load exact plugin name, return error otherwise */
static int talp_backend_manager_load(const char* plugin_name) {

    const char *dlb_plugin_path = get_dlb_plugin_path();
    if (dlb_plugin_path == NULL || dlb_plugin_path[0] == '\0') {
        return DLB_ERR_UNKNOWN;
    }

#ifndef DLB_TEST_LIB
    const char *plugin_suffix = "";
    int dlopen_flag = RTLD_LAZY;
#else
    /* stub libraries contain symbols that will be called during testing,
     * so we'll load the plugin and all symbols from its linked librarties. */
    const char *plugin_suffix = "_test";
    int dlopen_flag = RTLD_NOW | RTLD_GLOBAL;
#endif

    // Load the plugin
    char plugin_path[PATH_MAX];
    int not_truncated_len = snprintf(plugin_path, PATH_MAX, "%s/libdlb_%s%s.so",
            dlb_plugin_path, plugin_name, plugin_suffix);
    if (not_truncated_len >= PATH_MAX) {
        // really unlikely, but avoids GCC's format-truncation warning
        return DLB_ERR_UNKNOWN;
    }
    void *handle = dlopen(plugin_path, dlopen_flag);
    if (handle == NULL) {
        debug_warning("Failed to load plugin %s: %s", plugin_name, dlerror());
        return DLB_ERR_UNKNOWN;
    }

    // Load the plugin public function
    typedef backend_api_t* (*backend_get_api_func_t)(void);
    backend_get_api_func_t get_api = (backend_get_api_func_t)dlsym(handle, "DLB_Get_Backend_API");
    if (get_api == NULL) {
        debug_warning("Failed to find function: %s", dlerror());
        goto close_handle_and_error;
    }

    // Check that DLB_Get_Backend_API returns a valid API
    const backend_api_t *backend_api = get_api();
    if (backend_api == NULL) {
        debug_warning("DLB_Get_Backend_API returned NULL in plugin %s", plugin_name);
        goto close_handle_and_error;
    }

    // Check ABI version
    if (backend_api->abi_version != DLB_BACKEND_ABI_VERSION
            || backend_api->struct_size != sizeof(backend_api_t)) {
        debug_warning("ABI compatibility check failed in plugin %s", plugin_name);
        goto close_handle_and_error;
    }

    // Check that plugin return its own name
    if (strcmp(backend_api->name, plugin_name) != 0) {
        debug_warning("Plugin %s did not return an equivalent plugin name during initialization."
                " Deactivating...", plugin_name);
        goto close_handle_and_error;
    }

    // Check plugin capabilities
    if (backend_api->capabilities.gpu) {
        loaded_gpu_plugin.api = backend_api;
        loaded_gpu_plugin.handle = handle;
    } else if (backend_api->capabilities.hwc) {
        loaded_hwc_plugin.api = backend_api;
        loaded_hwc_plugin.handle = handle;
    } else {
        debug_warning("Unkown capabilities for plugin %s", plugin_name);
        goto close_handle_and_error;
    }

    return DLB_SUCCESS;

close_handle_and_error:
    dlclose(handle);
    return DLB_ERR_UNKNOWN;
}

const backend_api_t* talp_backend_manager_load_gpu_backend(const char *name) {

    if (loaded_gpu_plugin.handle == NULL) {
        if (name == NULL || name[0] == '\0') {
            // Try all possible GPU plugins until one succeeds
            for (size_t i = 0; i < num_gpu_plugins; ++i) {
                const char *p = gpu_plugins[i];
                if (talp_backend_manager_load(p) == DLB_SUCCESS) {
                    break;
                }
            }
        } else {
            talp_backend_manager_load(name);
        }
    }

    // Note that it may be NULL if no GPU plugin could be loaded
    return loaded_gpu_plugin.api;
}

void talp_backend_manager_unload_gpu_backend(void) {

    if (loaded_gpu_plugin.handle != NULL) {
        dlclose(loaded_gpu_plugin.handle);
        loaded_gpu_plugin = (const plugin_t){};
    }
}

const backend_api_t* talp_backend_manager_load_hwc_backend(const char *name) {

    if (loaded_hwc_plugin.handle == NULL) {
        if (name == NULL || name[0] == '\0') {
            // Try all possible HWC plugins until one succeeds
            for (size_t i = 0; i < num_hwc_plugins; ++i) {
                const char *p = hwc_plugins[i];
                if (talp_backend_manager_load(p) == DLB_SUCCESS) {
                    break;
                }
            }
        } else {
            talp_backend_manager_load(name);
        }
    }

    // Note that it may be NULL if no HWC plugin could be loaded
    return loaded_hwc_plugin.api;
}

void talp_backend_manager_unload_hwc_backend(void) {

    if (loaded_hwc_plugin.handle != NULL) {
        dlclose(loaded_hwc_plugin.handle);
        loaded_hwc_plugin = (const plugin_t){};
    }
}

void* talp_backend_manager_get_symbol_from_plugin(const char *symbol, const char *plugin_name) {

    if (loaded_gpu_plugin.handle != NULL
            && strcmp(loaded_gpu_plugin.api->name, plugin_name) == 0) {
        return dlsym(loaded_gpu_plugin.handle, symbol);
    }

    if (loaded_hwc_plugin.handle != NULL
            && strcmp(loaded_hwc_plugin.api->name, plugin_name) == 0) {
        return dlsym(loaded_hwc_plugin.handle, symbol);
    }

    return NULL;
}

int talp_backend_manager_get_gpu_affinity(char *buffer, size_t buffer_size, bool full_uuid) {

    // This function is only called from `dlb` utilility, so we don't expect
    // having the plugin already loaded, but we can support it just in case
    bool plugin_needs_loading = loaded_gpu_plugin.handle == NULL;

    if (plugin_needs_loading) {
        talp_backend_manager_load_gpu_backend(NULL);
    }

    int error = DLB_ERR_UNKNOWN;
    if (loaded_gpu_plugin.handle != NULL
            && loaded_gpu_plugin.api->get_gpu_affinity) {
        int backend_error = loaded_gpu_plugin.api->get_gpu_affinity(buffer, buffer_size, full_uuid);
        if (backend_error == DLB_BACKEND_SUCCESS) {
            error = DLB_SUCCESS;
        }
    }

    if (plugin_needs_loading) {
        talp_backend_manager_unload_gpu_backend();
    }

    return error;
}
