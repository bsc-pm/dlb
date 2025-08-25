/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "support/mask_utils.h"

#include "support/debug.h"
#include "support/dlb_common.h"

#ifdef HWLOC_LIB
#include <hwloc.h>
#include <hwloc/bitmap.h>
#include <hwloc/glibc-sched.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

#ifdef IS_BGQ_MACHINE
static void parse_mask_from_file(const char *filename, cpu_set_t *mask)
    __attribute__((unused));
static int parse_hwloc(void) __attribute__((unused));
static void parse_system_files(void) __attribute__((unused));
#endif


/*********************************************************************************/
/*    mu_cpuset_t: custom cpuset type for mask utils                             */
/*********************************************************************************/

/* Initial values to accomodate up to CPU_SETSIZE CPUs.
 * Later, they are reduced according to the machine specification */
static unsigned int mu_cpuset_setsize = CPU_SETSIZE;
static size_t mu_cpuset_alloc_size = CPU_ALLOC_SIZE(CPU_SETSIZE);
static size_t mu_cpuset_num_ulongs = CPU_ALLOC_SIZE(CPU_SETSIZE) / sizeof(unsigned long);


static inline void mu_cpuset_from_glibc_sched_affinity(mu_cpuset_t *mu_cpuset,
        const cpu_set_t *cpu_set) {
    *mu_cpuset = (const mu_cpuset_t) {
        .set = CPU_ALLOC(mu_cpuset_setsize),
        .alloc_size = mu_cpuset_alloc_size,
        .count = CPU_COUNT_S(mu_cpuset_alloc_size, cpu_set),
        .first_cpuid = mu_get_first_cpu(cpu_set),
        .last_cpuid = mu_get_last_cpu(cpu_set),
    };
    memcpy(mu_cpuset->set, cpu_set, mu_cpuset_alloc_size);
}

#ifdef HWLOC_LIB
static inline void mu_cpuset_from_hwloc_bitmap(mu_cpuset_t *mu_cpuset,
        hwloc_const_bitmap_t bitmap, hwloc_topology_t topology) {
    *mu_cpuset = (const mu_cpuset_t) {
        .set = CPU_ALLOC(mu_cpuset_setsize),
        .alloc_size = mu_cpuset_alloc_size,
        .count = hwloc_bitmap_weight(bitmap),
        .first_cpuid = hwloc_bitmap_first(bitmap),
        .last_cpuid = hwloc_bitmap_last(bitmap),
    };
    hwloc_cpuset_to_glibc_sched_affinity(topology, bitmap, mu_cpuset->set,
            mu_cpuset_alloc_size);
}
#endif


/*********************************************************************************/
/*    Mask utils system info                                                     */
/*********************************************************************************/

/* mask_utils main structure with system info */
typedef struct {
    unsigned int  num_nodes;
    unsigned int  num_cores;
    unsigned int  num_cpus;
    mu_cpuset_t   sys_mask;
    mu_cpuset_t*  node_masks;
    mu_cpuset_t*  core_masks_by_coreid;
    mu_cpuset_t** core_masks_by_cpuid;
} mu_system_loc_t;

enum { BITS_PER_BYTE = 8 };
enum { CPUS_PER_ULONG = sizeof(unsigned long) * BITS_PER_BYTE };

static mu_system_loc_t sys = {0};
static bool mu_initialized = false;

static void init_mu_struct(void) {
    sys = (const mu_system_loc_t) {};
}

/* This function (re-)initializes 'sys' with the given cpu sets.
 * It is used for specific set-ups, fallback, or testing purposes */
static void init_system_masks(const cpu_set_t *sys_mask,
        const cpu_set_t *core_masks, unsigned int num_cores,
        const cpu_set_t *node_masks, unsigned int num_nodes) {

    /* De-allocate structures if already initialized */
    if (mu_initialized) {
        mu_finalize();
    } else {
        init_mu_struct();
    }

    /*** System ***/
    sys.num_cpus = mu_get_last_cpu(sys_mask) + 1;
    mu_cpuset_setsize = sys.num_cpus;
    mu_cpuset_num_ulongs = (mu_cpuset_setsize + CPUS_PER_ULONG - 1) / CPUS_PER_ULONG;
    mu_cpuset_alloc_size = mu_cpuset_num_ulongs * sizeof(unsigned long);
    sys.sys_mask = (const mu_cpuset_t) {
        .set = CPU_ALLOC(mu_cpuset_setsize),
        .count = CPU_COUNT_S(mu_cpuset_alloc_size, sys_mask),
        .first_cpuid = 0,
        .last_cpuid = mu_cpuset_setsize - 1,
    };
    memcpy(sys.sys_mask.set, sys_mask, mu_cpuset_alloc_size);

    /*** Cores ***/
    sys.num_cores = num_cores;
    sys.core_masks_by_coreid = malloc(sys.num_cores * sizeof(mu_cpuset_t));
    sys.core_masks_by_cpuid = calloc(sys.num_cpus, sizeof(mu_cpuset_t*));
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        mu_cpuset_from_glibc_sched_affinity(core_cpuset, &core_masks[core_id]);
        for (int cpuid = core_cpuset->first_cpuid;
                cpuid >= 0 && cpuid != DLB_CPUID_INVALID;
                cpuid = mu_get_next_cpu(core_cpuset->set, cpuid)) {
            /* Save reference to another array indexed by cpuid */
            sys.core_masks_by_cpuid[cpuid] = core_cpuset;
        }
    }

    /*** NUMA Nodes ***/
    sys.num_nodes = num_nodes;
    sys.node_masks = malloc(sys.num_nodes * sizeof(mu_cpuset_t));
    for (unsigned int node_id = 0; node_id < sys.num_nodes; ++node_id) {
        mu_cpuset_from_glibc_sched_affinity(&sys.node_masks[node_id], &node_masks[node_id]);
    }

    mu_initialized = true;
}


/* This function (re-)initializes 'sys' given an overall number of resources.
 * It is used for specific set-ups, fallback, or testing purposes */
static void init_system(unsigned int num_cpus, unsigned int num_cores,
        unsigned int num_nodes) {

    /*** System ***/
    cpu_set_t sys_mask;
    CPU_ZERO(&sys_mask);
    for (unsigned int cpuid = 0; cpuid < num_cpus; ++cpuid) {
        CPU_SET(cpuid, &sys_mask);
    }

    /*** Cores ***/
    unsigned int cpus_per_core = num_cpus / num_cores;
    cpu_set_t *core_masks = calloc(num_cores, sizeof(cpu_set_t));
    for (unsigned int core_id = 0; core_id < num_cores; ++core_id) {
        for (unsigned int cpuid = core_id * cpus_per_core;
                cpuid < (core_id+1) * cpus_per_core; ++cpuid) {
            CPU_SET(cpuid, &core_masks[core_id]);
        }
    }

    /*** NUMA Nodes ***/
    unsigned int cpus_per_node = num_cpus / num_nodes;
    cpu_set_t *node_masks = calloc(num_nodes, sizeof(cpu_set_t));
    for (unsigned int node_id = 0; node_id < num_nodes; ++node_id) {
        for (unsigned int cpuid = node_id * cpus_per_node;
                cpuid < (node_id+1) * cpus_per_node; ++cpuid) {
            CPU_SET(cpuid, &node_masks[node_id]);
        }
    }

    init_system_masks(&sys_mask, core_masks, num_cores, node_masks, num_nodes);
    free(core_masks);
    free(node_masks);
}

static int parse_hwloc(void) {
#ifdef HWLOC_LIB
    /* Check runtime library compatibility */
    unsigned int hwloc_version = hwloc_get_api_version();
    if (hwloc_version >> 16 != HWLOC_API_VERSION >> 16) {
        warning("Detected incompatible HWLOC runtime library");
        return -1;
    }

    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    /*** System ***/
    hwloc_obj_t machine = hwloc_get_obj_by_type(topology, HWLOC_OBJ_MACHINE, 0);
    sys.num_cpus = hwloc_bitmap_last(machine->cpuset) + 1;
    mu_cpuset_setsize = sys.num_cpus;
#if HWLOC_API_VERSION >= 0x00020100
    mu_cpuset_num_ulongs = hwloc_bitmap_nr_ulongs(machine->cpuset);
#else
    mu_cpuset_num_ulongs = (mu_cpuset_setsize + CPUS_PER_ULONG - 1) / CPUS_PER_ULONG;
#endif
    mu_cpuset_alloc_size = mu_cpuset_num_ulongs * sizeof(unsigned long);
    mu_cpuset_from_hwloc_bitmap(&sys.sys_mask, machine->cpuset, topology);

    /*** Cores ***/
    hwloc_obj_type_t core = HWLOC_OBJ_CORE;
    sys.num_cores = hwloc_get_nbobjs_by_type(topology, core);
    sys.core_masks_by_coreid = calloc(sys.num_cores, sizeof(mu_cpuset_t));
    sys.core_masks_by_cpuid = calloc(sys.num_cpus, sizeof(mu_cpuset_t*));
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        hwloc_obj_t obj = hwloc_get_obj_by_type(topology, core, core_id);
        mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        mu_cpuset_from_hwloc_bitmap(core_cpuset, obj->cpuset, topology);
        for (int cpuid = core_cpuset->first_cpuid;
                cpuid >= 0 && cpuid != DLB_CPUID_INVALID;
                cpuid = hwloc_bitmap_next(obj->cpuset, cpuid)) {
            /* Save reference to another array indexed by cpuid */
            sys.core_masks_by_cpuid[cpuid] = core_cpuset;
        }
    }

    /*** NUMA Nodes ***/
    hwloc_obj_type_t node = HWLOC_OBJ_NODE;
    sys.num_nodes = hwloc_get_nbobjs_by_type(topology, node);
    sys.node_masks = calloc(sys.num_nodes, sizeof(mu_cpuset_t));
    for (unsigned int node_id = 0; node_id < sys.num_nodes; ++node_id) {
        hwloc_obj_t obj = hwloc_get_obj_by_type(topology, node, node_id);
        mu_cpuset_from_hwloc_bitmap(&sys.node_masks[node_id],
                obj->cpuset, topology);
    }

    hwloc_topology_destroy(topology);

    return 0;
#else
    return -1;
#endif
}

static void parse_mask_from_file(const char *filename, cpu_set_t *mask) {
    if (access(filename, F_OK) == 0) {
        enum { BUF_LEN = CPU_SETSIZE*7 };
        char buf[BUF_LEN];
        FILE *fd = fopen(filename, "r");

        if (!fgets(buf, BUF_LEN, fd)) {
            fatal("cannot read %s\n", filename);
        }
        fclose(fd);

        size_t len = strlen(buf);
        if (buf[len - 1] == '\n')
            buf[len - 1] = '\0';

        mu_parse_mask(buf, mask);
    }
}

static int parse_int_from_file(const char *filename) {
    int value = -1;
    if (access(filename, F_OK) == 0) {
        enum { BUF_LEN = 16 };
        char buf[BUF_LEN];
        FILE *fd = fopen(filename, "r");

        if (!fgets(buf, BUF_LEN, fd)) {
            fatal("cannot read %s\n", filename);
        }
        fclose(fd);

        value = strtol(buf, NULL, 10);
    }
    return value;
}

static int cmp_mu_cpuset(const void *a, const void *b) {
    const mu_cpuset_t *set_a = a;
    const mu_cpuset_t *set_b = b;

    if (set_a->count == 0) return 1;
    if (set_b->count == 0) return -1;
    return set_a->first_cpuid - set_b->first_cpuid;
}

#define PATH_SYSTEM_MASK "/sys/devices/system/cpu/present"
#define PATH_SYSTEM_CPUS "/sys/devices/system/cpu"
#define PATH_SYSTEM_NODE "/sys/devices/system/node"
static void parse_system_files(void) {
    /*** System ***/
    cpu_set_t system_mask;
    parse_mask_from_file(PATH_SYSTEM_MASK, &system_mask);
    sys.num_cpus = mu_get_last_cpu(&system_mask) + 1;
    mu_cpuset_setsize = sys.num_cpus;
    mu_cpuset_num_ulongs = (mu_cpuset_setsize + CPUS_PER_ULONG - 1) / CPUS_PER_ULONG;
    mu_cpuset_alloc_size = mu_cpuset_num_ulongs * sizeof(unsigned long);
    mu_cpuset_from_glibc_sched_affinity(&sys.sys_mask, &system_mask);

    /*** Cores ***/

    /* This array contains references to core_masks. It is initialized to 0
     * to account for possible missing CPU ids */
    sys.core_masks_by_cpuid = calloc(sys.num_cpus, sizeof(mu_cpuset_t*));

    /* The number of cores is not known beforehand, and the number indicates
     * the capacity to hold the last core id, rather than the valid cores */
    int num_cores = 0;

    /* Only if core_id info is not reliable */
    bool core_masks_array_needs_reordering = false;

    cpu_set_t empty_mask;
    CPU_ZERO(&empty_mask);

    DIR *dir = opendir(PATH_SYSTEM_CPUS);
    if (dir) {
        struct dirent *d;
        while ((d = readdir(dir))) {
            if (d && d->d_type == DT_DIR
                    && strncmp(d->d_name, "cpu", 3) == 0
                    && isdigit(d->d_name[3]) ) {

                enum { SYSPATH_MAX = 64 };
                char filename[SYSPATH_MAX];

                /* Get CPU id */
                int cpu_id = strtol(d->d_name+3, NULL, 10);
                fatal_cond(cpu_id < 0, "Error parsing cpu_id");

                /* Get core id */
                snprintf(filename, SYSPATH_MAX, PATH_SYSTEM_CPUS
                        "/%.8s/topology/core_id", d->d_name);
                int core_id = parse_int_from_file(filename);

                /* Get core CPUs list */
                cpu_set_t core_mask;
                CPU_ZERO(&core_mask);
                snprintf(filename, SYSPATH_MAX, PATH_SYSTEM_CPUS
                        "/%.8s/topology/thread_siblings_list", d->d_name);
                parse_mask_from_file(filename, &core_mask);

                /* Reallocate core_masks_by_coreid as needed */
                if (core_id >= num_cores) {
                    /* Realloc, saving prev_num_cores  */
                    int prev_num_cores = num_cores;
                    num_cores = core_id + 1;
                    void *p = realloc(sys.core_masks_by_coreid,
                            num_cores * sizeof(mu_cpuset_t));
                    fatal_cond(!p, "realloc failed in %s", __func__);
                    sys.core_masks_by_coreid = p;

                    /* Everytime we reallocate, we need to initialize
                     * [prev_size..new_size] with empty elements */
                    for (int i = prev_num_cores; i < num_cores; ++i) {
                        mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[i];
                        mu_cpuset_from_glibc_sched_affinity(core_cpuset, &empty_mask);
                    }
                }

                /* Save core mask */
                mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
                if (core_cpuset->count == 0) {
                    /* Save core mask */
                    mu_cpuset_from_glibc_sched_affinity(core_cpuset, &core_mask);
                } else if (CPU_EQUAL_S(mu_cpuset_alloc_size, core_cpuset->set, &core_mask)) {
                    /* Core mask already saved */
                } else {
                    /* If we get here it means that different masks have same
                     * core_id, so we cannot trust its value to index masks.
                     * Use whatever core_id to save the mask and then reorder array
                     * with virtual core_ids */

                    /* Find first unused core_id and whether this mask is also
                     * present with another core id */
                    int new_coreid = -1;
                    bool already_present = false;
                    for (int c = 0; c < num_cores; ++c) {
                        mu_cpuset_t *core = &sys.core_masks_by_coreid[c];
                        if (core->count == 0) {
                            if (new_coreid == -1) {
                                new_coreid = c;
                            }
                        } else if (CPU_EQUAL_S(mu_cpuset_alloc_size, core->set, &core_mask)) {
                            already_present = true;
                            break;
                        }
                    }

                    /* Continue to next CPU if the core mask has already been registered */
                    if (already_present) {
                        continue;
                    }

                    /* Relloacate if we didn't find en empty spot nor the same mask */
                    if (new_coreid == -1) {
                        new_coreid = num_cores++;
                        void *p = realloc(sys.core_masks_by_coreid,
                                num_cores * sizeof(mu_cpuset_t));
                        fatal_cond(!p, "realloc failed in %s", __func__);
                        sys.core_masks_by_coreid = p;
                    }

                    /* Finally save core mask */
                    mu_cpuset_t *new_core = &sys.core_masks_by_coreid[new_coreid];
                    mu_cpuset_from_glibc_sched_affinity(new_core, &core_mask);

                    /* Set boolean to do post-processing */
                    core_masks_array_needs_reordering = true;
                }
            }
        }
        closedir(dir);
    }
    sys.num_cores = num_cores;

    /* Only if replacing core_id by virtual core id */
    if (core_masks_array_needs_reordering) {
        qsort(sys.core_masks_by_coreid, num_cores, sizeof(mu_cpuset_t), cmp_mu_cpuset);
    }

    /* Add core mask references to array indexed by CPU id */
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        for (int cpuid = core_cpuset->first_cpuid;
                cpuid >= 0 && cpuid != DLB_CPUID_INVALID;
                cpuid = mu_get_next_cpu(core_cpuset->set, cpuid)) {
            sys.core_masks_by_cpuid[cpuid] = core_cpuset;
        }
    }

    /*** NUMA Nodes ***/
    int num_nodes = 0;
    dir = opendir(PATH_SYSTEM_NODE);
    if (dir) {
        struct dirent *d;
        while ((d = readdir(dir))) {
            if (d && d->d_type == DT_DIR
                    && strncmp(d->d_name, "node", 4) == 0
                    && isdigit(d->d_name[4]) ) {

                /* Get node id */
                int node_id = strtol(d->d_name+4, NULL, 10);
                fatal_cond(node_id < 0 || node_id > 1024, "Error parsing node_id");

                /* Get node CPUs list */
                cpu_set_t node_mask;
                CPU_ZERO(&node_mask);
                char filename[64];
                snprintf(filename, 64, PATH_SYSTEM_NODE "/%.10s/cpulist", d->d_name);
                parse_mask_from_file(filename, &node_mask);

                /* Save node mask */
                if (CPU_COUNT(&node_mask) > 0) {
                    num_nodes = max_int(num_nodes, node_id + 1);
                    mu_cpuset_t *p = realloc(sys.node_masks, num_nodes*sizeof(mu_cpuset_t));
                    fatal_cond(!p, "realloc failed");
                    sys.node_masks = p;
                    mu_cpuset_from_glibc_sched_affinity(&sys.node_masks[node_id], &node_mask);
                }
            }
        }
        closedir(dir);
    }
    sys.num_nodes = num_nodes;

    /* Fallback if some info could not be parsed */
    if (sys.sys_mask.count == 0) {
        int nproc_onln = sysconf(_SC_NPROCESSORS_ONLN);
        fatal_cond(nproc_onln <= 0, "Cannot obtain system size. Contact us at "
                PACKAGE_BUGREPORT " or configure DLB with HWLOC support.");
        init_system(nproc_onln, nproc_onln, 1);
    }
}


/*********************************************************************************/
/*    Mask utils public functions                                                */
/*********************************************************************************/

void mu_init( void ) {
    if ( !mu_initialized ) {
        init_mu_struct();

#if defined IS_BGQ_MACHINE
        enum { BGQ_NUM_CPUS  = 64 };
        enum { BGQ_NUM_CORES = 16 };
        enum { BGQ_NUM_NODES = 1 };
        init_system(BGQ_NUM_CPUS, BGQ_NUM_CORES, BGQ_NUM_NODES);
#else
        /* Try to parse HW info from HWLOC first */
        if (parse_hwloc() != 0) {
            /* Fallback to system files if needed */
            parse_system_files();
        }

        mu_initialized = true;
#endif
    }
}

/* This function used to be declared as destructor but it may be dangerous
 * with the OpenMP / DLB finalization at destruction time. */
void mu_finalize( void ) {

    CPU_FREE(sys.sys_mask.set);

    /* Nodes */
    for (unsigned int i = 0; i < sys.num_nodes; ++i) {
        CPU_FREE(sys.node_masks[i].set);
    }
    free(sys.node_masks);

    /* Cores per core id */
    for (unsigned int i = 0; i < sys.num_cores; ++i) {
        CPU_FREE(sys.core_masks_by_coreid[i].set);
    }
    free(sys.core_masks_by_coreid);

    /* Cores per CPU id (just references) */
    free(sys.core_masks_by_cpuid);

    sys = (const mu_system_loc_t) {};
    mu_initialized = false;
    mu_cpuset_setsize = CPU_SETSIZE;
    mu_cpuset_alloc_size = CPU_ALLOC_SIZE(CPU_SETSIZE);
    mu_cpuset_num_ulongs = CPU_ALLOC_SIZE(CPU_SETSIZE) / sizeof(unsigned long);
}

int mu_get_system_size( void ) {
    if (unlikely(!mu_initialized)) mu_init();
    return sys.sys_mask.last_cpuid + 1;
}

void mu_get_system_mask(cpu_set_t *mask) {
    if (unlikely(!mu_initialized)) mu_init();
    CPU_ZERO(mask);
    memcpy(mask, sys.sys_mask.set, mu_cpuset_alloc_size);
}

int mu_get_system_hwthreads_per_core(void) {
    if (unlikely(!mu_initialized)) mu_init();
    fatal_cond(sys.core_masks_by_coreid[0].count == 0,
            "Number of hardware threads per core is 0. Please report bug at " PACKAGE_BUGREPORT);
    return sys.core_masks_by_coreid[0].count;
}

int  mu_get_system_num_nodes(void) {
    return sys.num_nodes;
}

int  mu_get_system_cores_per_node(void) {
    return mu_count_cores(sys.node_masks[0].set);
}

void mu_get_system_description(print_buffer_t *buffer) {

    #define TAB "    "

    /* Should be enough to hold up a list of > 1024 CPUs */
    enum { LINE_BUFFER_SIZE = 8096 };
    char line[LINE_BUFFER_SIZE];
    char *l;

    printbuffer_init(buffer);

    // CPUs: N (C cores x T threads)
    l = line;
    l += sprintf(line, TAB"CPUs: %d", mu_get_system_size());
    if (mu_system_has_smt()) {
        sprintf(l, " (%d cores x %d threads)",
                mu_get_num_cores(), mu_get_system_hwthreads_per_core());
    } else {
        sprintf(l, " (%d cores)", mu_get_num_cores());
    }
    printbuffer_append(buffer, line);

    // NUMA nodes: N (C cores per NUMA node)
    l = line;
    l += sprintf(line, TAB"NUMA nodes: %d", mu_get_system_num_nodes());
    if (sys.num_nodes > 1) {
        sprintf(l, " (%d cores per NUMA node)", mu_get_system_cores_per_node());
    }
    printbuffer_append(buffer, line);

    // System mask
    printbuffer_append_no_newline(buffer, TAB"System mask: ");
    printbuffer_append(buffer, mu_to_str(sys.sys_mask.set));

    // NUMA node masks
    printbuffer_append_no_newline(buffer, TAB"NUMA node masks: ");
    for (unsigned int node_id = 0; node_id < sys.num_nodes; ++node_id) {
        printbuffer_append_no_newline(buffer, mu_to_str(sys.node_masks[node_id].set));
        if (node_id + 1 < sys.num_nodes) {
            printbuffer_append_no_newline(buffer, ", ");
        }
    }
    printbuffer_append(buffer, "");

    // Core masks
    printbuffer_append_no_newline(buffer, TAB"Core masks: ");
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        const mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        if (core_cpuset->count > 0) {
            printbuffer_append_no_newline(buffer, mu_to_str(core_cpuset->set));
            if (core_id + 1 < sys.num_cores) {
                printbuffer_append_no_newline(buffer, ", ");
            }
        }
    }
    printbuffer_append(buffer, "");
}

bool mu_system_has_smt(void) {
    if (unlikely(!mu_initialized)) mu_init();
    return sys.core_masks_by_coreid[0].count > 1;
}

int mu_get_num_cores(void) {
    if (unlikely(!mu_initialized)) mu_init();

    /* Not likely, but sys.num_cores may not be the number of valid cores */
    int num_cores = 0;
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        if (sys.core_masks_by_coreid[core_id].count > 0) {
            ++num_cores;
        }
    }

    return num_cores;
}

int mu_get_core_id(int cpuid) {

    if (cpuid < 0 || (unsigned)cpuid >= sys.num_cpus) return -1;

    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        if (CPU_ISSET_S(cpuid, mu_cpuset_alloc_size,
                    sys.core_masks_by_coreid[core_id].set)) {
            return core_id;
        }
    }

    return -1;
}

const mu_cpuset_t* mu_get_core_mask(int cpuid) {

    if (cpuid < 0 || (unsigned)cpuid >= sys.num_cpus) return NULL;

    return sys.core_masks_by_cpuid[cpuid];
}

const mu_cpuset_t* mu_get_core_mask_by_coreid(int core_id) {

    if (core_id < 0 || (unsigned)core_id >= sys.num_cores) return NULL;

    return &sys.core_masks_by_coreid[core_id];
}

/* Return Mask of full NUMA nodes covering at least 1 CPU of cpuset:
 * e.g.:
 *  node0: [0-3]
 *  node1: [4-7]
 *  cpuset: [1-7]
 *  returns [0-7]
 */
void mu_get_nodes_intersecting_with_cpuset(cpu_set_t *node_set, const cpu_set_t *cpuset) {

    CPU_ZERO(node_set);
    for (unsigned int i=0; i<sys.num_nodes; ++i) {
        cpu_set_t intxn;
        CPU_AND_S(mu_cpuset_alloc_size, &intxn, sys.node_masks[i].set, cpuset);
        if (CPU_COUNT_S(mu_cpuset_alloc_size, &intxn) > 0) {
            CPU_OR_S(mu_cpuset_alloc_size, node_set, node_set, sys.node_masks[i].set);
        }
    }
}

/* Return Mask of full NUMA nodes containing all CPUs in cpuset:
 * e.g.:
 *  node0: [0-3]
 *  node1: [4-7]
 *  cpuset: [1-7]
 *  returns [4-7]
 */
void mu_get_nodes_subset_of_cpuset(cpu_set_t *node_set, const cpu_set_t *cpuset) {

    CPU_ZERO(node_set);
    for (unsigned int i=0; i<sys.num_nodes; ++i) {
        if (mu_is_subset(sys.node_masks[i].set, cpuset)) {
            CPU_OR_S(mu_cpuset_alloc_size, node_set, node_set, sys.node_masks[i].set);
        }
    }
}

/* Return Mask of cores covering at least 1 CPU of cpuset:
 * e.g.:
 *  node0: [0-1]
 *  node1: [2-3]
 *  cpuset: [1-3]
 *  returns [0-3]
 */
void mu_get_cores_intersecting_with_cpuset(cpu_set_t *core_set, const cpu_set_t *cpuset) {
    CPU_ZERO(core_set);
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        const mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        cpu_set_t intxn;
        CPU_AND_S(mu_cpuset_alloc_size, &intxn, core_cpuset->set, cpuset);
        if (CPU_COUNT_S(mu_cpuset_alloc_size, &intxn) > 0) {
            CPU_OR_S(mu_cpuset_alloc_size, core_set, core_set, core_cpuset->set);
        }
    }
}

/* Return Mask of cores containing all CPUs in cpuset:
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  cpuset: [1-3]
 *  returns [2-3]
 */
void mu_get_cores_subset_of_cpuset(cpu_set_t *core_set, const cpu_set_t *cpuset) {
    CPU_ZERO(core_set);
    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        const mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        if (mu_is_subset(core_cpuset->set, cpuset)) {
            CPU_OR_S(mu_cpuset_alloc_size, core_set, core_set, core_cpuset->set);
        }
    }
}

/* Return the next enabled CPU in mask which pertains to the next core after
 * prev_cpu, or -1 if not found. */
int mu_get_cpu_next_core(const cpu_set_t *mask, int prev_cpu) {

    if (unlikely(prev_cpu < -1)) return -1;

    int prev_core = mu_get_core_id(prev_cpu);
    int next_cpu = mu_get_next_cpu(mask, prev_cpu);
    int next_core = mu_get_core_id(next_cpu);

    while (next_cpu != -1
            && next_core <= prev_core) {
        next_cpu = mu_get_next_cpu(mask, next_cpu);
        next_core = mu_get_core_id(next_cpu);
    }

    return (next_cpu == -1 || (unsigned)next_cpu >= sys.num_cpus) ? -1 : next_cpu;
}

/* We define as "complete" those cores that all the CPUs defined by
 * sys.core_masks_by_coreid are enabled. */

/* Return the number of complete cores in the mask.
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: [0-4]
 *  returns 2
 */
int mu_count_cores(const cpu_set_t *mask) {

    int cores_count = 0;

    for (unsigned int coreid = 0; coreid < sys.num_cores; coreid++) {
        // Check if we have the complete set of CPUs form the core
        if (mu_is_subset(sys.core_masks_by_coreid[coreid].set, mask)) {
            cores_count++;
        }
    }

    return cores_count;
}

/* Return the number of complete cores in the mask.
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: [0-4]
 *  returns 3
 */
int mu_count_cores_intersecting_with_cpuset(const cpu_set_t *mask) {

    int cores_count = 0;

    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        const mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        cpu_set_t intxn;
        CPU_AND_S(mu_cpuset_alloc_size, &intxn, core_cpuset->set, mask);
        if (CPU_COUNT_S(mu_cpuset_alloc_size, &intxn) > 0) {
            cores_count++;
        }
    }

    return cores_count;
}

/* Return the id of the last complete core in the mask if any, otherwise return -1.
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: [0-3]
 *  returns 1 (node1)
 */
int mu_get_last_coreid(const cpu_set_t *mask){
    for (int coreid = sys.num_cores-1; coreid >= 0 ; coreid--) {
        // Check if we have the complete set of CPUs form the core
        if (mu_is_subset(sys.core_masks_by_coreid[coreid].set, mask)) {
            return coreid;
        }
    }

    return -1;
}

/* Disables the CPUs of the last complete core in the mask and returns its
 * coreid if any, otherwise return -1.
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: [2-5]
 *  returns 2 (node2)
 *  updated cpuset: [2-3]
 */
int mu_take_last_coreid(cpu_set_t *mask) {
    int last_coreid = mu_get_last_coreid(mask);
    if (last_coreid == -1) return -1;
    mu_xor(mask, mask, sys.core_masks_by_coreid[last_coreid].set);
    return last_coreid;
}

/* Enables all the CPUs of the core
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: []
 *  coreid: 1
 *  updated cpuset: [2-3]
 */
void mu_set_core(cpu_set_t *mask, int coreid){
    mu_or(mask, mask, sys.core_masks_by_coreid[coreid].set);
}

/* Disables all the CPUs of the core
 * e.g.:
 *  core0: [0-1]
 *  core1: [2-3]
 *  core2: [4-5]
 *  cpuset: [0-5]
 *  coreid: 1
 *  updated cpuset: [0-1,4-5]
 */
void mu_unset_core(cpu_set_t *mask, int coreid){
    mu_subtract(mask, mask, sys.core_masks_by_coreid[coreid].set);
}

/* Basic mask utils functions that do not need to read system's topology,
 * i.e., mostly mask operations */

void mu_zero(cpu_set_t *result) {
    CPU_ZERO_S(mu_cpuset_alloc_size, result);
}

void mu_and(cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2) {
    CPU_AND_S(mu_cpuset_alloc_size, result, mask1, mask2);
}

void mu_or(cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2) {
    CPU_OR_S(mu_cpuset_alloc_size, result, mask1, mask2);
}

void mu_xor (cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2) {
    CPU_XOR_S(mu_cpuset_alloc_size, result, mask1, mask2);
}

bool mu_equal(const cpu_set_t *mask1, const cpu_set_t *mask2) {
    return CPU_EQUAL_S(mu_cpuset_alloc_size, mask1, mask2) != 0;
}

/* Returns true is all bits in subset are set in superset */
bool mu_is_subset(const cpu_set_t *subset, const cpu_set_t *superset) {
    // The condition is true if the intersection is identical to subset
    cpu_set_t intxn;
    CPU_AND_S(mu_cpuset_alloc_size, &intxn, subset, superset);
    return CPU_EQUAL_S(mu_cpuset_alloc_size, &intxn, subset);
}

/* Returns true is all bits in superset are set in subset */
bool mu_is_superset(const cpu_set_t *superset, const cpu_set_t *subset) {
    // The condition is true if the intersection is identical to subset
    cpu_set_t intxn;
    CPU_AND_S(mu_cpuset_alloc_size, &intxn, superset, subset);
    return CPU_EQUAL_S(mu_cpuset_alloc_size, &intxn, subset);
}

/* Returns true is all bits in subset are set in superset and they're not equal */
bool mu_is_proper_subset(const cpu_set_t *subset, const cpu_set_t *superset) {
    cpu_set_t intxn;
    CPU_AND_S(mu_cpuset_alloc_size, &intxn, subset, superset);
    return CPU_EQUAL_S(mu_cpuset_alloc_size, &intxn, subset)
        && !CPU_EQUAL_S(mu_cpuset_alloc_size, subset, superset);
}

/* Returns true is all bits in superset are set in subset and they're not equal */
bool mu_is_proper_superset(const cpu_set_t *superset, const cpu_set_t *subset) {
    cpu_set_t intxn;
    CPU_AND_S(mu_cpuset_alloc_size, &intxn, superset, subset);
    return CPU_EQUAL_S(mu_cpuset_alloc_size, &intxn, subset)
        && !CPU_EQUAL_S(mu_cpuset_alloc_size, superset, subset);
}

/* Return true if any bit is present in both sets */
bool mu_intersects(const cpu_set_t *mask1, const cpu_set_t *mask2) {
    cpu_set_t intxn;
    CPU_AND_S(mu_cpuset_alloc_size, &intxn, mask1, mask2);
    return CPU_COUNT_S(mu_cpuset_alloc_size, &intxn) > 0;
}

/* Return the number of bits set in mask */
int mu_count(const cpu_set_t *mask) {
    return CPU_COUNT_S(mu_cpuset_alloc_size, mask);
}

/* Return the minuend after subtracting the bits in subtrahend */
void mu_subtract(cpu_set_t *result, const cpu_set_t *minuend, const cpu_set_t *subtrahend) {
    cpu_set_t xor;
    CPU_XOR_S(mu_cpuset_alloc_size, &xor, minuend, subtrahend);
    CPU_AND_S(mu_cpuset_alloc_size, result, minuend, &xor);
}

/* Return the one and only enabled CPU in mask, or -1 if count != 1 */
int mu_get_single_cpu(const cpu_set_t *mask) {
    if (CPU_COUNT_S(mu_cpuset_alloc_size, mask) == 1) {
        return mu_get_first_cpu(mask);
    }
    return -1;
}

/* some of the following functions have been inspired by:
 * https://github.com/open-mpi/hwloc/blob/master/hwloc/bitmap.c */

/* Return the first enabled CPU in mask, or -1 if mask is empty */
int mu_get_first_cpu(const cpu_set_t *mask) {

    for (unsigned int i = 0; i < mu_cpuset_num_ulongs; ++i) {
        unsigned long bits = mask->__bits[i];
        if (bits) {
            return ffsl(bits) - 1 + CPUS_PER_ULONG * i;
        }
    }

    return -1;
}

/* Return the last enabled CPU in mask, or -1 if mask is empty */
int mu_get_last_cpu(const cpu_set_t *mask) {

    for (unsigned int i = mu_cpuset_num_ulongs; i-- > 0; ) {
        unsigned long bits = mask->__bits[i];
        if (bits) {
            /* glibc does not provide a fls function, there are more optimal
             * solutions, but this function is not that critical */
            int cpuid = CPUS_PER_ULONG * i;
            while (bits >>= 1) {
                ++cpuid;
            }
            return cpuid;
        }
    }

    return -1;
}

/* Return the next enabled CPU in mask after prev, or -1 if not found */
int mu_get_next_cpu(const cpu_set_t *mask, int prev) {

    if (unlikely(prev < -1)) return -1;

    for (unsigned int i = (prev + 1) / CPUS_PER_ULONG;
            i < mu_cpuset_num_ulongs; ++i) {
        unsigned long bits = mask->__bits[i];

        /* mask bitmap only if previous cpu belong to current iteration */
        if (prev >= 0 && (unsigned)prev / CPUS_PER_ULONG == i) {
            bits &= ULONG_MAX << (prev % CPUS_PER_ULONG + 1);
        }

        if (bits) {
            return ffsl(bits) - 1 + CPUS_PER_ULONG * i;
        }
    }

    return -1;
}

/* Return the next unset CPU in mask after prev, or -1 if not found */
int mu_get_next_unset(const cpu_set_t *mask, int prev) {

    if (unlikely(prev < -1)) return -1;

    for (unsigned int i = (prev + 1) / CPUS_PER_ULONG;
            i < mu_cpuset_num_ulongs; ++i) {
        unsigned long bits = ~(mask->__bits[i]);

        /* mask bitmap only if previous cpu belong to current iteration */
        if (prev >= 0 && (unsigned)prev / CPUS_PER_ULONG == i) {
            bits &= ULONG_MAX << (prev % CPUS_PER_ULONG + 1);
        }

        if (bits) {
            return ffsl(bits) - 1 + CPUS_PER_ULONG * i;
        }
    }

    return -1;
}

// mu_to_str and mu_parse_mask functions are used by DLB utilities
// We export their dynamic symbols to avoid code duplication,
// although they do not belong to the public API
DLB_EXPORT_SYMBOL
const char* mu_to_str( const cpu_set_t *mask ) {

    static __thread char buffer[CPU_SETSIZE*4];
    char *b = buffer;
    *(b++) = '[';
    bool entry_made = false;
    for (int cpuid = mu_get_first_cpu(mask); cpuid >= 0;
            cpuid = mu_get_next_cpu(mask, cpuid)) {

        /* Find interval distance */
        int next_unset = mu_get_next_unset(mask, cpuid);
        int distance = next_unset > 0 ? next_unset - 1 - cpuid
            : mu_get_last_cpu(mask) - cpuid;

        /* Add ',' separator for subsequent entries */
        if (entry_made) {
            *(b++) = ',';
        } else {
            entry_made = true;
        }

        /* Write element, pair or range */
        if (distance == 0) {
            b += sprintf(b, "%d", cpuid);
        } else if (distance == 1) {
            b += sprintf(b, "%d,%d", cpuid, cpuid+1);
            ++cpuid;
        } else {
            b += sprintf(b, "%d-%d", cpuid, cpuid+distance);
            cpuid += distance;
        }
    }
    *(b++) = ']';
    *b = '\0';

    return buffer;
}

static void parse_64_bits_mask(cpu_set_t *mask, unsigned int offset, const char *str, int base) {
    unsigned long long number = strtoull(str, NULL, base);
    unsigned int i = offset;
    while (number > 0 && i < mu_cpuset_setsize) {
        if (number & 1) {
            CPU_SET(i, mask);
        }
        ++i;
        number = number >> 1;
    }
}

DLB_EXPORT_SYMBOL
void mu_parse_mask( const char *str, cpu_set_t *mask ) {
    if (!str) return;

    size_t str_len = strnlen(str, CPU_SETSIZE+1);
    if (str_len > CPU_SETSIZE) return;

    CPU_ZERO( mask );
    if (str_len == 0) return;

    regex_t regex_bitmask;
    regex_t regex_hexmask;
    regex_t regex_range;
    regex_t old_regex_bitmask;

    /* Compile regular expressions */
    if ( regcomp(&regex_bitmask, "^0[bB][0-1]+$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }
    if ( regcomp(&regex_hexmask, "^0[xX][0-9,a-f,A-F]+$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }
    if ( regcomp(&regex_range, "^[0-9,-]+$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }

    /***** Deprecated *****/
    if ( regcomp(&old_regex_bitmask, "^[0-1][0-1]+[bB]$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }
    /* Regular expression matches OLD bitmask, e.g.: 11110011b */
    if ( !regexec(&old_regex_bitmask, str, 0, NULL, 0) ) {
        warning("The binary form xxxxb is deprecated, please use 0bxxxx.");
        // Parse
        for (unsigned int i=0; i<str_len; i++) {
            if ( str[i] == '1' && i < mu_cpuset_setsize ) {
                CPU_SET( i, mask );
            }
        }
    }
    /**********************/

    /* Regular expression matches bitmask, e.g.: 0b11100001 */
    else if ( !regexec(&regex_bitmask, str, 0, NULL, 0) ) {
        /* Ignore '0b' */
        str += 2;
        if (strlen(str) <= 64) {
            parse_64_bits_mask(mask, 0, str, 2);
        } else {
            /* parse in chunks of 64 bits */
            char *str_copy = strdup(str);
            char *start_ptr;
            char *end_ptr = str_copy + strlen(str_copy);
            unsigned int offset = 0;
            do {
                start_ptr = strlen(str_copy) < 64 ? str_copy : end_ptr - 64;
                parse_64_bits_mask(mask, offset, start_ptr, 2);
                offset += 64;
                end_ptr = start_ptr;
                *end_ptr = '\0';
            } while (strlen(str_copy) > 0);
            free(str_copy);
        }
    }

    /* Regular expression matches hexmask, e.g.: 0xE1 */
    else if ( !regexec(&regex_hexmask, str, 0, NULL, 0) ) {
        /* Ignore '0x' */
        str += 2;
        if (strlen(str) <= 16) {
            parse_64_bits_mask(mask, 0, str, 16);
        } else {
            /* parse in chunks of 64 bits (16 hex digits) */
            char *str_copy = strdup(str);
            char *start_ptr;
            char *end_ptr = str_copy + strlen(str_copy);
            unsigned int offset = 0;
            do {
                start_ptr = strlen(str_copy) < 16 ? str_copy : end_ptr - 16;
                parse_64_bits_mask(mask, offset, start_ptr, 16);
                offset += 64;
                end_ptr = start_ptr;
                *end_ptr = '\0';
            } while (strlen(str_copy) > 0);
            free(str_copy);
        }
    }

    /* Regular expression matches range, e.g.: 0,5-7 */
    else if ( !regexec(&regex_range, str, 0, NULL, 0) ) {
        // Parse
        const char *ptr = str;
        char *endptr;
        while ( ptr < str+strlen(str) ) {
            // Discard junk at the left
            if ( !isdigit(*ptr) ) { ptr++; continue; }

            unsigned long start_ = strtoul( ptr, &endptr, 10 );
            unsigned long start = start_ < mu_cpuset_setsize ? start_ : mu_cpuset_setsize;
            ptr = endptr;

            // Single element
            if ( (*ptr == ',' || *ptr == '\0') && start < mu_cpuset_setsize ) {
                CPU_SET( start, mask );
                ptr++;
                continue;
            }
            // Range
            else if ( *ptr == '-' ) {
                // Discard '-' and possible junk
                ptr++;
                if ( !isdigit(*ptr) ) { ptr++; continue; }

                unsigned long end_ = strtoul( ptr, &endptr, 10 );
                unsigned long end = end_ < mu_cpuset_setsize ? end_ : mu_cpuset_setsize;
                ptr = endptr;

                // Valid range
                if ( end > start ) {
                    for ( unsigned long i=start; i<=end && i<mu_cpuset_setsize; i++ ) {
                        CPU_SET( i, mask );
                    }
                }
                continue;
            }
            // Unexpected token
            else { }
        }
    }
    /* Regular expression does not match */
    else { }

    regfree(&regex_bitmask);
    regfree(&regex_hexmask);
    regfree(&regex_range);
    regfree(&old_regex_bitmask);

    if ( CPU_COUNT(mask) == 0 ) {
        warning( "Parsed mask \"%s\" does not seem to be a valid mask\n", str );
    }
}

/* Equivalent to mu_to_str, but generate quoted string in str up to namelen-1 bytes */
void mu_get_quoted_mask(const cpu_set_t *mask, char *str, size_t namelen) {
    if (namelen < 2)
        return;

    char *b = str;
    *(b++) = '"';
    size_t bytes = 1;
    bool entry_made = false;
    for (int cpuid = mu_get_first_cpu(mask); cpuid >= 0;
            cpuid = mu_get_next_cpu(mask, cpuid)) {

        /* Find interval distance */
        int next_unset = mu_get_next_unset(mask, cpuid);
        int distance = next_unset > 0 ? next_unset - 1 - cpuid
            : mu_get_last_cpu(mask) - cpuid;

        /* Add ',' separator for subsequent entries */
        if (entry_made) {
            if (bytes+1 < namelen) {
                *(b++) = ',';
                ++bytes;
            }
        } else {
            entry_made = true;
        }

        /* Write element, pair or range */
        if (distance == 0) {
            int len = snprintf(NULL, 0, "%d", cpuid);
            if (bytes+len < namelen) {
                b += sprintf(b, "%d", cpuid);
                bytes += len;
            }
        } else if (distance == 1) {
            int len = snprintf(NULL, 0, "%d,%d", cpuid, cpuid+1);
            if (bytes+len < namelen) {
                b += sprintf(b, "%d,%d", cpuid, cpuid+1);
                bytes += len;
                ++cpuid;
            }
        } else {
            int len = snprintf(NULL, 0, "%d-%d", cpuid, cpuid+distance);
            if (bytes+len < namelen) {
                b += sprintf(b, "%d-%d", cpuid, cpuid+distance);
                bytes += len;
                cpuid += distance;
            }
        }
    }
    if (bytes+1 < namelen) {
        *(b++) = '"';
        ++bytes;
    }
    *b = '\0';
}

char * mu_parse_to_slurm_format(const cpu_set_t *mask) {
    char *str = malloc((mu_cpuset_setsize >> 2) + 3);
    if (str < 0)
        return NULL;
    unsigned int offset = 2;
    unsigned long long val = 0;
    const int threshold = 4;
    sprintf(str, "0x");
    for (int cpuid = mu_get_last_cpu(mask); cpuid >= 0; --cpuid) {
        if(CPU_ISSET(cpuid, mask)) {
            val |= 1 << (cpuid % threshold);
        }
        if (cpuid > 0 && cpuid % threshold == 0) {
            sprintf(str+offset, "%llx", val);
            val = 0;
            offset++;
        }
    }
    sprintf(str+offset, "%llx", val);
    return str;
}

bool mu_equivalent_masks(const char *str1, const char *str2) {
    cpu_set_t mask1, mask2;
    mu_parse_mask(str1, &mask1);
    mu_parse_mask(str2, &mask2);
    return CPU_EQUAL(&mask1, &mask2);
}


static int cmp_cpuids(cpuid_t cpuid1, cpuid_t cpuid2) {
    int cpu1_core_id = mu_get_core_id(cpuid1);
    int cpu2_core_id = mu_get_core_id(cpuid2);
    if (cpu1_core_id == cpu2_core_id) {
        return cpuid1 - cpuid2;
    } else {
        return cpu1_core_id - cpu2_core_id;
    }
}

/* Compare CPUs so that:
 *  - owned CPUs first, in ascending order
 *  - non-owned later, starting from the first owned, then ascending
 *  e.g.: system: [0-7], owned: [3-5]
 *      cpu_list = {4,5,6,7,0,1,2,3}
 */
int mu_cmp_cpuids_by_ownership(const void *cpuid1, const void *cpuid2, void *mask) {
    /* Expand arguments */
    cpuid_t _cpuid1 = *(cpuid_t*)cpuid1;
    cpuid_t _cpuid2 = *(cpuid_t*)cpuid2;
    cpu_set_t *process_mask = mask;

    if (CPU_ISSET(_cpuid1, process_mask)) {
        if (CPU_ISSET(_cpuid2, process_mask)) {
            /* both CPUs are owned: ascending order */
            return cmp_cpuids(_cpuid1, _cpuid2);
        } else {
            /* cpuid2 is NOT owned and cpuid1 IS */
            return -1;
        }
    } else {
        if (CPU_ISSET(_cpuid2, process_mask)) {
            /* cpuid2 IS owned and cpuid1 is NOT */
            return 1;
        } else {
            /* none is owned */
            int first_cpu = mu_get_first_cpu(process_mask);
            int first_core = mu_get_core_id(first_cpu);
            int cpu1_core_id = mu_get_core_id(_cpuid1);
            int cpu2_core_id = mu_get_core_id(_cpuid2);
            if ((cpu1_core_id > first_core
                        && cpu2_core_id > first_core)
                    || (cpu1_core_id < first_core
                        && cpu2_core_id < first_core)) {
                /* Both CPUs are either before or after the process mask */
                return cmp_cpuids(_cpuid1, _cpuid2);
            } else {
                /* Compare with respect to process mask */
                return cmp_cpuids(first_cpu, _cpuid1);
            }
        }
    }
}

/* Compare CPUs so that:
 *  - CPUs are sorted according to the affinity array:
 *      * affinity: array of cpu_set_t, each position represents a level
 *                  in the affinity, the last position is an empty cpu set.
 *      (PRE: each affinity level is a superset of the previous level mask)
 *  - Sorted by affinity level in ascending order
 *  - non-owned later, starting from the first owned, then ascending
 *  e.g.: affinity: {{6-7}, {4-7}, {0-7}, {}}
 *      sorted_cpu_list = {6,7,4,5,0,1,2,3}
 */
int mu_cmp_cpuids_by_affinity(const void *cpuid1, const void *cpuid2, void *affinity) {
    /* Expand arguments */
    cpuid_t _cpuid1 = *(cpuid_t*)cpuid1;
    cpuid_t _cpuid2 = *(cpuid_t*)cpuid2;
    cpu_set_t *_affinity = affinity;

    /* Find affinity level of each CPU */
    int cpu1_level = 0;
    int cpu2_level = 0;
    cpu_set_t *mask = _affinity;
    while(CPU_COUNT(mask) > 0) {
        if (!CPU_ISSET(_cpuid1, mask)) {
            ++cpu1_level;
        }
        if (!CPU_ISSET(_cpuid2, mask)) {
            ++cpu2_level;
        }
        ++mask;
    }

    /* If levels differ, sort levels in ascending order */
    if (cpu1_level != cpu2_level) {
        return cpu1_level - cpu2_level;
    }

    /* If both are level 0, sort in ascending order */
    if (cpu1_level == 0) {
        return cmp_cpuids(_cpuid1, _cpuid2);
    }

    /* If both are level 1, sort from the first CPU in level 0 */
    /* e.g.: level0: [2,3], level1: [0,7] -> [4,5,6,7,0,1] */
    if (cpu1_level == 1) {
        cpu_set_t *level0_mask = _affinity;
        int first_cpu = mu_get_first_cpu(level0_mask);
        int first_core = mu_get_core_id(first_cpu);
        int cpu1_core_id = mu_get_core_id(_cpuid1);
        int cpu2_core_id = mu_get_core_id(_cpuid2);
        if ((cpu1_core_id > first_core
                    && cpu2_core_id > first_core)
                || (cpu1_core_id < first_core
                    && cpu2_core_id < first_core)) {
            /* Both CPUs are either before or after the process mask */
            return cmp_cpuids(_cpuid1, _cpuid2);
        } else {
            /* Compare with respect to process mask */
            return cmp_cpuids(first_cpu, _cpuid1);
        }
    }

    /* TODO: compute numa distance */
    /* Levels 2+, sort in ascending order */
    return cmp_cpuids(_cpuid1, _cpuid2);
}


/*********************************************************************************/
/*    Mask utils testing functions                                               */
/*********************************************************************************/

static void print_sys_info(void) {

    verbose(VB_AFFINITY, "System mask: %s", mu_to_str(sys.sys_mask.set));

    for (unsigned int node_id = 0; node_id < sys.num_nodes; ++node_id) {
        verbose(VB_AFFINITY, "Node %d mask: %s",
                node_id, mu_to_str(sys.node_masks[node_id].set));
    }

    for (unsigned int core_id = 0; core_id < sys.num_cores; ++core_id) {
        const mu_cpuset_t *core_cpuset = &sys.core_masks_by_coreid[core_id];
        if (core_cpuset->count > 0) {
            verbose(VB_AFFINITY, "Core %d mask: %s",
                    core_id, mu_to_str(core_cpuset->set));
        }
    }

    for (int cpuid = sys.sys_mask.first_cpuid;
            cpuid >= 0 && cpuid != DLB_CPUID_INVALID;
            cpuid = mu_get_next_cpu(sys.sys_mask.set, cpuid)) {
        const mu_cpuset_t *core_cpuset = sys.core_masks_by_cpuid[cpuid];
        if (core_cpuset && core_cpuset->count > 0) {
            verbose(VB_AFFINITY, "CPU %d core mask: %s",
                    cpuid, mu_to_str(core_cpuset->set));
        }
    }
}

bool mu_testing_is_initialized(void) {
    return mu_initialized;
}

void mu_testing_set_sys_size(int size) {
    init_system(size, size, 1);
    print_sys_info();
}

void mu_testing_set_sys(unsigned int num_cpus, unsigned int num_cores,
        unsigned int num_nodes) {
    init_system(num_cpus, num_cores, num_nodes);
    print_sys_info();
}

void mu_testing_set_sys_masks(const cpu_set_t *sys_mask,
        const cpu_set_t *core_masks, unsigned int num_cores,
        const cpu_set_t *node_masks, unsigned int num_nodes) {
    init_system_masks(sys_mask, core_masks, num_cores, node_masks, num_nodes);
    print_sys_info();
}

void mu_testing_init_nohwloc(void) {
    init_mu_struct();
    parse_system_files();
    mu_initialized = true;
    print_sys_info();
}
