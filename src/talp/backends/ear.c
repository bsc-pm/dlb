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

#include <common/hardware/topology.h>
#include <common/states.h>
#include <common/types/configuration/cluster_conf.h>

/* DLB's own build already passes -D_GNU_SOURCE; eard_api_conf.h #defines it
 * again unconditionally, which -Werror turns fatal on redefinition. */
#undef _GNU_SOURCE
#include <daemon/local_api/eard_api.h>
#include <metrics/common/apis.h>
#include <metrics/energy_cpu/energy_cpu.h>
#include <metrics/gpu/gpu.h>

#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <dlfcn.h>

static const core_api_t *dlb_core_api = NULL;

/*********************************************************************************/
/* CPU/DRAM - EAR energy state                                                   */
/*********************************************************************************/

static bool ear_plugin_initialized = false;  	// set once init() fully succeeds
static bool ear_plugin_started = false;      	// set once start() runs

static unsigned int ear_api = API_NONE;      	// EAR backend

static topology_t ear_tp;                    	// CPU/DRAM topology
static unsigned int ear_dev_count = 0;       	// Number of CPU sockets/packages
static unsigned long long *ear_sample = NULL; 	// Raw per-socket readings


static int cpu_dram_energy_init(topology_t *tp)
{
    // Load the CPU/DRAM backend
    if (energy_cpu_load(tp, ear_api) != EAR_SUCCESS) {
        PLUGIN_ERROR("EAR energy backend: energy_cpu_load failed: %s\n", state_msg);
        return DLB_BACKEND_ERROR;
    }

    // Initialize it
    if (energy_cpu_init(no_ctx) != EAR_SUCCESS) {
        PLUGIN_ERROR("EAR energy backend: energy_cpu_init failed: %s\n", state_msg);
        return DLB_BACKEND_ERROR;
    }

    // Confirm at least one real device exists
    if (energy_cpu_count_devices(no_ctx, &ear_dev_count) != EAR_SUCCESS || ear_dev_count == 0) {
        PLUGIN_ERROR("EAR energy backend: energy_cpu_count_devices failed: %s\n", state_msg);
        energy_cpu_dispose(no_ctx);
        return DLB_BACKEND_ERROR;
    }

    // Allocate ear_sample, sized for ear_dev_count sockets
    if (energy_cpu_data_alloc(no_ctx, &ear_sample, NULL) != EAR_SUCCESS) {
        PLUGIN_ERROR("EAR energy backend: energy_cpu_data_alloc failed: %s\n", state_msg);
        energy_cpu_dispose(no_ctx);
        return DLB_BACKEND_ERROR;
    }

    return DLB_BACKEND_SUCCESS;
}

static bool read_cpu_dram_energy(energy_readings_t *readings)
{
    int64_t cpu_total = 0;
    int64_t dram_total = 0;
    unsigned int i;

    if (energy_cpu_read(no_ctx, ear_sample) != EAR_SUCCESS) {
        PLUGIN_WARNING("EAR energy backend: energy_cpu_read failed: %s\n", state_msg);
        return false;
    }

    /* ear_sample layout: DRAM of socket i is ear_sample[i], CPU (package) of
     * socket i is ear_sample[ear_dev_count + i] */
    for (i = 0; i < ear_dev_count; ++i) {
        cpu_total  += ear_sample[ear_dev_count + i];
        dram_total += ear_sample[i];
    }

    // Energy measured in nJ
    readings->cpu = cpu_total;
    readings->dram = dram_total;

    return true;
}


/*********************************************************************************/
/* Node - EAR energy state                                                       */
/*********************************************************************************/

/* EAR node energy state (whole-node reading, independent of CPU/DRAM).
 * Talks to the node-energy plugin's raw symbols directly (dlopen/dlsym)
 * instead of going through EAR's own energy_node.h loader, which misbehaves
 * in this process. */

// Function pointer declaration for the plugin's energy_init()
typedef state_t (*raw_energy_init_fn)(void **c);
typedef state_t (*raw_energy_dispose_fn)(void **c);
typedef state_t (*raw_energy_datasize_fn)(size_t *size);
typedef state_t (*raw_energy_dc_read_fn)(void *c, void *energy_data);
typedef state_t (*raw_energy_accumulated_fn)(unsigned long *e, void *init, void *end);
typedef state_t (*raw_energy_units_fn)(uint *units);

// Actual function pointers
static raw_energy_init_fn        node_raw_init        = NULL;
static raw_energy_dispose_fn     node_raw_dispose     = NULL;
static raw_energy_datasize_fn    node_raw_datasize    = NULL;
static raw_energy_dc_read_fn     node_raw_dc_read     = NULL;
static raw_energy_accumulated_fn node_raw_accumulated = NULL;
static raw_energy_units_fn       node_raw_units       = NULL;

static bool node_energy_available = false;   // Set once node_energy_init() fully succeeds

static void *node_plugin_handle = NULL;      // dlopen() handle for the plugin .so
static void *node_ctx = NULL;                // Per-plugin context
static void *node_baseline = NULL;           // Node's baseline point, taken at start
static void *node_current = NULL;            // Latest sample and refreshed every read
static int64_t node_last_good = 0;           // Fallback value when a read fails
static size_t node_data_size = 0;            // Bytes per sample, plugin-specific
static uint node_units = 0;                  // Plugin's unit scale: 1=J, 1000=mJ, etc.


static void node_energy_init(void)
{
    cluster_conf_t conf;
    my_node_conf_t *node_conf = NULL;
    char ear_conf_path[SZ_PATH];
    char nodename[256];
    char energy_plugin_path[SZ_PATH];

    // Finds ear.conf via EAR's standard search locations/env var
    if (get_ear_conf_path(ear_conf_path) == EAR_ERROR) {
        PLUGIN_WARNING("EAR node energy: could not find ear.conf\n");
        return;
    }

    // Parses the whole cluster config into conf
    if (state_fail(read_cluster_conf(ear_conf_path, &conf))) {
        PLUGIN_WARNING("EAR node energy: could not read EAR configuration: %s\n", ear_conf_path);
        return;
    }

    // ear.conf is cluster-wide; need this hostname to find the entry
    if (gethostname(nodename, sizeof(nodename)) != 0) {
        PLUGIN_WARNING("EAR node energy: gethostname failed\n");
        free_cluster_conf(&conf);
        return;
    }
    // Drop domain suffix if hostname is something like node01.domain
    char *dot = strchr(nodename, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    // Look up this hostname's entry in the parsed config
    node_conf = get_my_node_conf(&conf, nodename);
    if (node_conf == NULL) {
        PLUGIN_WARNING("EAR node energy: no EAR node configuration found for %s\n", nodename);
        free_cluster_conf(&conf);
        return;
    }

    if (node_conf->energy_plugin == NULL || node_conf->energy_plugin[0] == '\0') {
        PLUGIN_WARNING("EAR node energy: no energy plugin configured for node %s\n", nodename);
        free(node_conf->policies);
        free(node_conf);
        free_cluster_conf(&conf);
        return;
    }

    // Plugin lives at <dir_plug>/energy/<name>
    int n = snprintf(energy_plugin_path, sizeof(energy_plugin_path), "%s/energy/%s",
            conf.install.dir_plug, node_conf->energy_plugin);

    free(node_conf->policies);
    free(node_conf);
    free_cluster_conf(&conf);

    // n >= size means snprintf silently truncated the path
    if (n < 0 || (size_t)n >= sizeof(energy_plugin_path)) {
        PLUGIN_WARNING("EAR node energy: energy plugin path too long\n");
        return;
    }

    // RTLD_NOW resolves all symbols immediately; RTLD_LOCAL keeps them private
    node_plugin_handle = dlopen(energy_plugin_path, RTLD_NOW | RTLD_LOCAL);
    if (node_plugin_handle == NULL) {
        PLUGIN_WARNING("EAR node energy: dlopen('%s') failed: %s\n",
                energy_plugin_path, dlerror());
        return;
    }

    // One lookup per symbol; dlsym only resolves one at a time
    node_raw_init        = (raw_energy_init_fn) dlsym(node_plugin_handle, "energy_init");
    node_raw_dispose     = (raw_energy_dispose_fn) dlsym(node_plugin_handle, "energy_dispose");
    node_raw_datasize    = (raw_energy_datasize_fn) dlsym(node_plugin_handle, "energy_datasize");
    node_raw_dc_read     = (raw_energy_dc_read_fn) dlsym(node_plugin_handle, "energy_dc_read");
    node_raw_accumulated = (raw_energy_accumulated_fn) dlsym(node_plugin_handle, "energy_accumulated");
    node_raw_units       = (raw_energy_units_fn) dlsym(node_plugin_handle, "energy_units");

    if (!node_raw_init || !node_raw_dispose || !node_raw_datasize
            || !node_raw_dc_read || !node_raw_accumulated || !node_raw_units) {
        PLUGIN_WARNING("EAR node energy: plugin '%s' is missing an expected symbol\n",
                energy_plugin_path);
        dlclose(node_plugin_handle);
        node_plugin_handle = NULL;
        return;
    }

    // First real call into the plugin
    if (node_raw_init(&node_ctx) != EAR_SUCCESS) {
        PLUGIN_WARNING("EAR node energy: energy_init failed\n");
        dlclose(node_plugin_handle);
        node_plugin_handle = NULL;
        return;
    }

    // Needed to convert the plugin's native unit to nJ later
    if (node_raw_units(&node_units) != EAR_SUCCESS || node_units == 0) {
        PLUGIN_WARNING("EAR node energy: energy_units failed\n");
        node_raw_dispose(&node_ctx);
        dlclose(node_plugin_handle);
        node_plugin_handle = NULL;
        return;
    }

    // How many bytes one opaque sample takes
    if (node_raw_datasize(&node_data_size) != EAR_SUCCESS || node_data_size == 0) {
        PLUGIN_WARNING("EAR node energy: energy_datasize failed\n");
        node_raw_dispose(&node_ctx);
        dlclose(node_plugin_handle);
        node_plugin_handle = NULL;
        return;
    }

    // Initialize node readings
    node_baseline = calloc(1, node_data_size);
    node_current = calloc(1, node_data_size);

    if (node_baseline == NULL || node_current == NULL) {
        PLUGIN_WARNING("EAR node energy: allocation failed\n");
        node_raw_dispose(&node_ctx);
        free(node_baseline);
        free(node_current);
        node_baseline = NULL;
        node_current = NULL;
        return;
    }

    node_energy_available = true;
}

static void read_node_energy(energy_readings_t *readings)
{
    readings->node = 0; // default when node isn't configured/available

    if (!node_energy_available) {
        return;
    }

    if (node_raw_dc_read(node_ctx, node_current) == EAR_SUCCESS) {
        unsigned long node_delta = 0;

        // Energy consumed between node_baseline and node_current
        node_raw_accumulated(&node_delta, node_baseline, node_current);

        // node_units: 1=J, 1000=mJ, 1000000=uJ, 1000000000=nJ; normalize to nJ
        readings->node = (int64_t)node_delta * (int64_t)(1000000000ULL / node_units);

        node_last_good = readings->node;
    } else {
        readings->node = node_last_good;
        PLUGIN_WARNING("EAR energy backend: node energy_dc_read failed\n");
    }
}


/*********************************************************************************/
/* GPU - EAR energy state                                                        */
/*********************************************************************************/

/* EAR GPU energy state (per-device, independent of CPU/DRAM/node).
 * Uses EAR's vendor-agnostic metrics/gpu API (NVML/RSMI/oneAPI under the
 * hood). Prefers a reachable EARD daemon and falls back to a direct hardware
 * read otherwise */

#define GPU_EARD_CONNECT_TIMEOUT_S 5

static bool gpu_energy_available = false;    // Set once gpu_energy_init() fully succeeds
static bool gpu_eard_connected = false;      // True if we're reading GPU energy via EARD

static uint gpu_dev_count = 0;               // Number of local GPU devices
static gpu_t *gpu_sample = NULL;             // Per-device readings, refreshed every read
static int64_t gpu_last_good = 0;            // Fallback value when a read fails


/* Not thread-local: only safe because gpu_select_backend() is only ever
 * called once, from one thread, during init. */
static sigjmp_buf gpu_eard_connect_jmp;

static void gpu_eard_connect_alarm_handler(int sig)
{
    // Jumps back to gpu_select_backend()'s sigsetjmp when SIGALRM fires
    (void) sig;
    siglongjmp(gpu_eard_connect_jmp, 1);
}

static int gpu_select_backend(void)
{
    /* eards_connection() should fail fast when no daemon is reachable, but
     * some deployments never ACK an unregistered connection and hang
     * indefinitely; bound the wait so a stuck daemon can't hang DLB */
    struct sigaction sa, old_sa; // sa = our handler, old_sa = previous one
    int connected = 0;

    // Install our handler, saving the previous one to restore afterward
    sa.sa_handler = gpu_eard_connect_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, &old_sa);

    if (sigsetjmp(gpu_eard_connect_jmp, 1) == 0) {
        /* First pass: attempt the connection under a timeout.
         * Known limitation: alarm() is a single, global, per-process timer;
         * this clobbers any alarm the host app already had pending. */
        alarm(GPU_EARD_CONNECT_TIMEOUT_S);
        connected = state_ok(eards_connection()) && eards_connected();
        alarm(0);
    } else {
        // Only reached if the alarm fired and jumped us back here
        PLUGIN_WARNING("EAR GPU energy: EARD connection attempt timed out, "
                "falling back to direct hardware read\n");
    }

    // Always restore the previous handler, success or not
    sigaction(SIGALRM, &old_sa, NULL);

    gpu_eard_connected = connected; // remembered so finalize() knows to disconnect
    return connected ? API_EARD : API_DEFAULT;
}

static void gpu_energy_init(void)
{
    // Picks EARD-backed or direct API
    gpu_load(gpu_select_backend());

    if (state_fail(gpu_init(no_ctx))) {
        PLUGIN_WARNING("EAR GPU energy: gpu_init failed: %s\n", state_msg);
        return;
    }

    // Counts CPU devices
    gpu_get_devices(NULL, &gpu_dev_count);
    if (gpu_dev_count == 0) {
        PLUGIN_WARNING("EAR GPU energy: no GPU devices found\n");
        gpu_dispose(no_ctx);
        return;
    }

    // Allocates the per-device sample buffer, sized for gpu_dev_count
    gpu_data_alloc(&gpu_sample);
    if (gpu_sample == NULL) {
        PLUGIN_WARNING("EAR GPU energy: gpu_data_alloc failed\n");
        gpu_dispose(no_ctx);
        return;
    }

    gpu_energy_available = true;
}

static void read_gpu_energy(energy_readings_t *readings)
{
    unsigned int i;
    int64_t gpu_total = 0;

    readings->gpu = 0;

    if (!gpu_energy_available) {
        return;
    }

    if (state_fail(gpu_read(no_ctx, gpu_sample))) {
        /* Repeat the last good value instead of 0, so the next successful
         * read doesn't diff against a stray zero and spike */
        readings->gpu = gpu_last_good;
        PLUGIN_WARNING("EAR GPU energy: gpu_read failed: %s\n", state_msg);
        return;
    }

    for (i = 0; i < gpu_dev_count; ++i) {
        // Skip devices currently reporting an invalid/failed reading
        if (gpu_sample[i].working && gpu_sample[i].correct) {
            gpu_total += (int64_t)(gpu_sample[i].energy_j * 1.0e9);
        }
    }

    readings->gpu = gpu_total;
    gpu_last_good = readings->gpu;
}


static int read_total_energy(energy_readings_t *readings)
{
    /* CPU/DRAM is mandatory, node and GPU are best-effort and never
     *  abort the whole read */
    if (readings == NULL) return DLB_BACKEND_ERROR;
    if (!ear_plugin_initialized) return DLB_BACKEND_ERROR;

    if (!read_cpu_dram_energy(readings)) {
        return DLB_BACKEND_ERROR;
    }

    read_node_energy(readings);
    read_gpu_energy(readings);

    return DLB_BACKEND_SUCCESS;
}

/*********************************************************************************/
/* Plugin functions                                                              */
/*********************************************************************************/

static int ear_backend_probe(void)
{
    state_t s;
    topology_t tp; 		// CPU/DRAM topology
    unsigned int count = 0;     // Number of CPU sockets/packages

    // Confirm topology detection works on this machine
    if (state_fail(s = topology_init(&tp))) {
        return DLB_BACKEND_ERROR;
    }

    // Try loading the CPU/DRAM backend
    if (energy_cpu_load(&tp, ear_api) != EAR_SUCCESS) {
        return DLB_BACKEND_ERROR;
    }

    // Try initializing backend
    if (energy_cpu_init(no_ctx) != EAR_SUCCESS) {
        return DLB_BACKEND_ERROR;
    }

    // Confirm at least one real device exists
    if (energy_cpu_count_devices(no_ctx, &count) != EAR_SUCCESS || count == 0) {
        energy_cpu_dispose(no_ctx);
        return DLB_BACKEND_ERROR;
    }

    return DLB_BACKEND_SUCCESS;
}

static int ear_backend_init(const core_api_t *core_api)
{
    state_t s;

    if (ear_plugin_initialized) return DLB_BACKEND_ERROR; // guard against double-init

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION ||
        dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    // Both are optional; init() continues regardless of their outcome
    node_energy_init();
    gpu_energy_init();

    // Get current topology 
    if (state_fail(s = topology_init(&ear_tp))) {
        PLUGIN_ERROR("EAR energy backend: topology_init failed: %s (%d)\n",
                     state_msg, s);
        return DLB_BACKEND_ERROR;
    }

    // Initialize CPU/DRAM, which are mandatory
    if (cpu_dram_energy_init(&ear_tp) != DLB_BACKEND_SUCCESS) {
        return DLB_BACKEND_ERROR;
    }

    ear_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

static int ear_backend_start(void)
{
    if (!ear_plugin_initialized) return DLB_BACKEND_ERROR;
    if (ear_plugin_started) return DLB_BACKEND_SUCCESS; // already started, not an error

    if (node_energy_available) {
        /* Takes node's baseline reading here; CPU/DRAM/GPU get theirs later,
         * in talp_energy_init() */
        if (node_raw_dc_read(node_ctx, node_baseline) != EAR_SUCCESS) {
            PLUGIN_WARNING("EAR energy backend: initial node energy reading failed\n");
        }
    }

    ear_plugin_started = true;

    PLUGIN_PRINT("EAR energy backend started\n");

    return DLB_BACKEND_SUCCESS;
}

static int ear_backend_energy_collect(energy_readings_t *out)
{
    if (out == NULL) return DLB_BACKEND_ERROR;
    if (!ear_plugin_started) return DLB_BACKEND_ERROR;

    return read_total_energy(out);
}

static int ear_backend_stop(void)
{
    ear_plugin_started = false;
    return DLB_BACKEND_SUCCESS;
}

static int ear_backend_finalize(void)
{
    if (!ear_plugin_initialized) return DLB_BACKEND_SUCCESS;

    // Release CPU/DRAM resources first
    energy_cpu_data_free(no_ctx, &ear_sample);
    energy_cpu_dispose(no_ctx);

    ear_sample = NULL;
    ear_dev_count = 0;
    ear_plugin_initialized = false;

    // Node_raw_dispose must run before dlclose() unloads it
    if (node_energy_available) {
        node_raw_dispose(&node_ctx);

        free(node_baseline);
        free(node_current);

        node_baseline = NULL;
        node_current = NULL;
        node_data_size = 0;
        node_units = 0;
        node_energy_available = false;
    }

    // Unloads the plugin .so itself
    if (node_plugin_handle != NULL) {
        dlclose(node_plugin_handle);
        node_plugin_handle = NULL;
    }

    // Mirror of gpu_energy_init() in reverse
    if (gpu_energy_available) {
        gpu_data_free(&gpu_sample);
        gpu_dispose(no_ctx);
        gpu_dev_count = 0;
        gpu_energy_available = false;
    }

    // Only disconnect if gpu_select_backend() actually connected
    if (gpu_eard_connected) {
        eards_disconnect();
        gpu_eard_connected = false;
    }

    return DLB_BACKEND_SUCCESS;
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void)
{
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "ear",
        .capabilities = {
            .energy = true,
        },
        .probe = ear_backend_probe,
        .init = ear_backend_init,
        .start = ear_backend_start,
        .stop = ear_backend_stop,
        .finalize = ear_backend_finalize,
        .energy = {
            .collect = ear_backend_energy_collect,
        },
    };
    return &api;
}
