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

#include "plugins/plugin_manager.h"

#include "apis/dlb_errors.h"
#include "plugins/plugin.h"
#include "support/debug.h"
#include "support/gslist.h"

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

// TODO: Some of the warning messages may actually be undesired if we are
//       just trying to load a plugin, or silently ignore if error
//       Consider a new --verbose=plugin option.


typedef struct plugin_t {
    plugin_info_t  info;
    plugin_api_t  *api;
    void          *handle;
} plugin_t;

GSList *loaded_plugins = NULL;

/* Obtain full path to libdlb.so (or whatever variant is loaded) */
static const char* get_dlb_lib_path(void) {

    static char dlb_lib_path[PATH_MAX] = "";

    // Note: we don't want to call dlsym twice, so just in case we want to load
    // more than one plugin, make sure that this function is only called once.
    if (dlb_lib_path[0] != '\0') return dlb_lib_path;

    // Get the path to the current library
    void *dlb_lib_handle = dlopen(NULL, RTLD_LAZY);
    if (!dlb_lib_handle) {
        debug_warning("dlopen failed to load the current library: %s", dlerror());
        return NULL;
    }

    // Get the address of any symbol in the DLB library
    void *dlb_init_fn = dlsym(dlb_lib_handle, "DLB_Init");
    if (!dlb_init_fn) {
        debug_warning("dlsym failed to look for DLB_Init address: %s", dlerror());
        return NULL;
    }

    // Get information of the dlb shared object currently loaded
    Dl_info info;
    if (dladdr(dlb_init_fn, &info)) {
        strncpy(dlb_lib_path, dirname((char*)info.dli_fname), PATH_MAX - 1);
        dlb_lib_path[PATH_MAX - 1] = '\0';
    } else {
        debug_warning("dladdr failed to obtain information");
        return NULL;
    }

    return dlb_lib_path;
}

/* Load exact plugin name, return error otherwise */
int load_plugin(const char* plugin_name) {

    const char *dlb_lib_path = get_dlb_lib_path();
    if (dlb_lib_path == NULL || dlb_lib_path[0] == '\0') {
        return DLB_ERR_UNKNOWN;
    }

    // Load the plugin
    char plugin_path[PATH_MAX];
    int not_truncated_len = snprintf(plugin_path, PATH_MAX, "%s/libdlb_%s.so",
            dlb_lib_path, plugin_name);
    if (not_truncated_len >= PATH_MAX) {
        // really unlikely, but avoids GCC's format-truncation warning
        return DLB_ERR_UNKNOWN;
    }
    void *handle = dlopen(plugin_path, RTLD_LAZY);
    if (!handle) {
        debug_warning("Failed to load plugin %s: %s", plugin_name, dlerror());
        return DLB_ERR_UNKNOWN;
    }

    // Load the plugin public function
    plugin_get_api_func_t get_api = (plugin_get_api_func_t)dlsym(handle, "DLB_Get_Plugin_API");
    if (!get_api) {
        debug_warning("Failed to find function: %s", dlerror());
        dlclose(handle);
        return DLB_ERR_UNKNOWN;
    }

    // Allocate memory for keeping the plugin's data
    plugin_t *plugin = malloc(sizeof(plugin_t));

    // Initialize structure (call DLB_Get_Plugin_API)
    *plugin = (const plugin_t) {
        .api = get_api(),
        .handle = handle,
    };

    // Init plugin
    if (plugin->api->init(&plugin->info) == DLB_PLUGIN_ERROR) {
        // deallocate failed plugin
        free(plugin);
        return DLB_ERR_UNKNOWN;
    }

    // Plugin must return its own name, for correct-checking and for unloading it later
    if (strcmp(plugin->info.name, plugin_name) != 0) {
        debug_warning("Plugin %s did not return an equivalent plugin name during initialization."
                " Deactivating...", plugin_name);
        plugin->api->finalize();
        free(plugin);
        return DLB_ERR_UNKNOWN;
    }

    // Add plugin to list for future reference
    loaded_plugins = g_slist_prepend(loaded_plugins, plugin);

    return DLB_SUCCESS;
}

int unload_plugin(const char* plugin_name) {

    int error = DLB_ERR_UNKNOWN;
    plugin_t *found_plugin = NULL;

    for (GSList *node = loaded_plugins;
            node != NULL;
            node = node->next) {

        plugin_t *plugin = node->data;
        if (strcmp(plugin->info.name, plugin_name) == 0) {
            /* found plugin, unload */
            found_plugin = plugin;
            error = plugin->api->finalize();
            dlclose(plugin->handle);
            break;
        }
    }

    /* plugin was found in the list, remove it */
    if (found_plugin != NULL) {
        loaded_plugins = g_slist_remove(loaded_plugins, found_plugin);
    }

    return error == DLB_PLUGIN_SUCCESS ? DLB_SUCCESS : error;
}

void load_plugins(const char *plugin_list) {

    if (plugin_list == NULL || plugin_list[0] == '\0') return;

    /* Tokenize a copy of the input string and initialize each plugin */
    size_t len = strlen(plugin_list) + 1;
    char *plugin_list_copy = malloc(sizeof(char)*len);
    strcpy(plugin_list_copy, plugin_list);
    char *end = NULL;
    char *token = strtok_r(plugin_list_copy, ",", &end);
    while(token != NULL) {
        if (load_plugin(token) != DLB_SUCCESS) {
            warning("Plugin \"%s\" could not be loaded", token);
        }
        token = strtok_r(NULL, ",", &end);
    }

    free(plugin_list_copy);
}

void unload_plugins(const char *plugin_list) {

    if (plugin_list == NULL || plugin_list[0] == '\0') return;

    /* Tokenize a copy of the input string and initialize each plugin */
    size_t len = strlen(plugin_list) + 1;
    char *plugin_list_copy = malloc(sizeof(char)*len);
    strcpy(plugin_list_copy, plugin_list);
    char *end = NULL;
    char *token = strtok_r(plugin_list_copy, ",", &end);
    while(token != NULL) {
        if (unload_plugin(token) != DLB_SUCCESS) {
            warning("Plugin \"%s\" could not be unloaded", token);
        }
        token = strtok_r(NULL, ",", &end);
    }

    free(plugin_list_copy);
}

void plugin_get_gpu_affinity(char *buffer, size_t buffer_size, bool full_uuid) {

    // check only first loaded plugin
    plugin_t *plugin = loaded_plugins ? loaded_plugins->data : NULL;
    if (plugin != NULL
            && plugin->api->get_affinity) {
        plugin->api->get_affinity(buffer, buffer_size, full_uuid);
    }
}

void* plugin_get_symbol_from_plugin(const char *symbol, const char *plugin_name) {

    void *symbol_addr = NULL;

    for (GSList *node = loaded_plugins;
            node != NULL;
            node = node->next) {

        plugin_t *plugin = node->data;
        if (strcmp(plugin->info.name, plugin_name) == 0) {
            /* found plugin, look for symbol */
            symbol_addr = dlsym(plugin->handle, "rocprofiler_configure");
            break;
        }
    }

    return symbol_addr;
}
