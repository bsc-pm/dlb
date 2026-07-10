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

#include "talp/regions.h"

#include "LB_comm/shmem_talp.h"
#include "LB_core/spd.h"
#include "LB_core/thread_ctx.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_talp.h"
#include "support/debug.h"
#include "support/gtree.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "talp/sample.h"
#include "talp/talp.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*********************************************************************************/
/*    Thread-local Monitoring Regions                                            */
/*********************************************************************************/

typedef struct {
    dlb_monitor_t  *monitor;    /* global region record to flush into at close */
    talp_sample_t   snapshot;   /* sample at open time; delta = close - snapshot */
    int             cpuid;      /* single CPU that participates in this region */
} local_region_entry_t;

__thread GSList *local_regions = NULL;

static int region_start_thread(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {

    monitor_data_t *monitor_data = monitor->_data;
    if (monitor_data->flags.started || !monitor_data->flags.enabled) {
        return DLB_NOUPDT;
    }

    talp_info_t *talp_info = spd->talp_info;

    /* Update this sample first */
    talp_sample_update(talp_info);

    verbose(VB_TALP, "Starting region %s", monitor->name);
    instrument_event(MONITOR_REGION, monitor_data->id, EVENT_BEGIN);

    const talp_sample_t *sample = talp_sample_get(spd->talp_info);

    /* For thread-local regions we can't modify the sample, we just keep a snapshot */
    local_region_entry_t *entry = malloc(sizeof(local_region_entry_t));
    *entry = (local_region_entry_t) {
        .monitor  = monitor,
        .snapshot = {
            .timers   = sample->timers,
            .counters = sample->counters,
            .stats    = sample->stats,
        },
        .cpuid = sched_getcpu(),
    };

    local_regions = g_slist_prepend(local_regions, entry);

    return DLB_SUCCESS;
}

static int region_stop_thread(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {

    local_region_entry_t *entry = NULL;

    /* Get region entry if monitor is a special value */
    if (monitor == DLB_LAST_OPEN_REGION) {
        if (local_regions != NULL) {
            entry = local_regions->data;
            monitor = entry->monitor;
        } else {
            return DLB_ERR_NOENT;
        }
    }

    /* Get region entry if monitor is an actual pointer */
    for (GSList *node = local_regions;
            node != NULL && entry == NULL;
            node = node->next) {

        local_region_entry_t *e = node->data;
        if (e->monitor == monitor) {
            entry = e;
        }
    }

    if (entry == NULL) return DLB_NOUPDT;

    talp_info_t *talp_info = spd->talp_info;

    /* Update this sample first */
    talp_sample_update(talp_info);

    verbose(VB_TALP, "Stopping region %s", monitor->name);
    instrument_event(MONITOR_REGION, ((monitor_data_t*)monitor->_data)->id, EVENT_END);

    /* Compute delta since this region was started */
    const talp_sample_t *sample = talp_sample_get(talp_info);
    talp_sample_t delta = talp_sample_delta(sample, &entry->snapshot);
    int64_t elapsed = sample->last_updated_ts - entry->snapshot.last_updated_ts;

    /* We don't want the previous CPU mask in the sample because we were not able to
     * reset it on local region start. Instead, we set the cpuid obtained at that point. */
    CPU_ZERO(&delta.cpu_mask);
    CPU_SET(entry->cpuid, &delta.cpu_mask);

    /* Aggregate delta to region */
    talp_aggregate_sample_to_region(talp_info, monitor, &delta, elapsed);

    local_regions = g_slist_remove(local_regions, entry);
    free(entry);

    return DLB_SUCCESS;
}


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

const char *global_region_name = DLB_GLOBAL_REGION_NAME;


/* Unique region ids */
static int get_new_monitor_id(void) {
    static atomic_int id = 0;
    return DLB_ATOMIC_ADD_FETCH_RLX(&id, 1);
}

/* Unique anonymous regions */
static int get_new_anonymous_id(void) {
    static atomic_int id = 0;
    return DLB_ATOMIC_ADD_FETCH_RLX(&id, 1);
}

/* Return true if the region is to be enabled.
 * region_select format:
 *  --talp-region-select=[(include|exclude):]<region-list>
 */
static bool parse_region_select(const char *region_select, const char *region_name) {

    /* Default case, all regions enabled */
    if (region_select == NULL
            || region_select[0] == '\0') {
        return true;
    }

    /* Select inclusion or exclusion mode,
     * and advance pointer */
    bool in_inclusion_mode = true;
    if (strncmp(region_select, "exclude:", strlen("exclude:")) == 0) {
       in_inclusion_mode = false;
       region_select += strlen("exclude:");
    } else if (strncmp(region_select, "include:", strlen("include:"))  == 0) {
       region_select += strlen("include:");
    }

    /* If "[(include|exclude):]all" */
    if (strcmp(region_select, "all") == 0) {
        return in_inclusion_mode;
    }

    /* Break region_select into tokens and find region_name */
    bool found_in_select = false;
    size_t len = strlen(region_select);
    char *region_select_copy = malloc(sizeof(char)*(len+1));
    strcpy(region_select_copy, region_select);
    char *saveptr = NULL;
    char *token = strtok_r(region_select_copy, ",", &saveptr);
    while (token) {
        /* Region name is found */
        if (strcmp(token, region_name) == 0) {
            found_in_select = true;
            break;
        }

        /* Region name is same as global region, and same as token (ignoring case) */
        if (strcasecmp(region_name, global_region_name) == 0
                && strcasecmp(token, global_region_name) == 0) {
            found_in_select = true;
            break;
        }

        /* next token */
        token = strtok_r(NULL, ",", &saveptr);
    }
    free(region_select_copy);

    return in_inclusion_mode ? found_in_select : !found_in_select;
}

static void region_initialize(dlb_monitor_t *monitor, int id, const
        char *name, pid_t pid, float avg_cpus, const char *region_select, bool have_shmem) {
    /* Initialize private monitor data */
    monitor_data_t *monitor_data = malloc(sizeof(monitor_data_t));
    *monitor_data = (const monitor_data_t) {
        .id = id,
        .node_shared_id = -1,
    };

    /* Parse --talp-region-select if needed */
    monitor_data->flags.enabled = parse_region_select(region_select, name);

    /* Allocate monitor name */
    char *allocated_name = malloc(DLB_MONITOR_NAME_MAX*sizeof(char));
    snprintf(allocated_name, DLB_MONITOR_NAME_MAX, "%s", name);

    /* Initialize monitor */
    *monitor = (const dlb_monitor_t) {
            .name = allocated_name,
            .avg_cpus = avg_cpus,
            ._data = monitor_data,
    };

    /* Register name in the instrumentation tool */
    instrument_register_event(MONITOR_REGION, monitor_data->id, name);

    /* Register region in shmem */
    if (have_shmem) {
        int err = shmem_talp__register(pid, avg_cpus, monitor->name, &monitor_data->node_shared_id);
        if (err == DLB_ERR_NOMEM) {
            warning("Region %s has been correctly registered but cannot be shared among other"
                    " processes due to lack of space in the TALP shared memory. Features like"
                    " node report or gathering data from external processes may not work for"
                    " this region. If needed, increase the TALP shared memory capacity using"
                    " the flag --shm-size-multiplier. Run dlb -hh for more info.",
                    monitor->name);
        } else if (err < DLB_SUCCESS) {
            fatal("Unknown error registering region %s, please report bug at %s",
                    monitor->name, PACKAGE_BUGREPORT);
        }
    }
}

struct dlb_monitor_t* region_get_global(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    return talp_info ? talp_info->monitor : NULL;
}

const char* region_get_global_name(void) {
    return global_region_name;
}

/* Helper function for GTree: Compare region names */
int region_compare_by_name(const void *a, const void *b) {
    return strncmp(a, b, DLB_MONITOR_NAME_MAX-1);
}

/* Helper function for GTree: deallocate */
void region_dealloc(void *data) {

    dlb_monitor_t *monitor = data;

    /* Free private data */
    monitor_data_t *monitor_data = monitor->_data;
    free(monitor_data);
    monitor_data = NULL;

    /* Free name */
    free((char*)monitor->name);
    monitor->name = NULL;

    /* Free monitor */
    free(monitor);
}

dlb_monitor_t* region_register(const subprocess_descriptor_t *spd, const char* name) {

    /* Forbidden names */
    if (name == DLB_LAST_OPEN_REGION
            || (name != NULL
                && strncasecmp("all", name, DLB_MONITOR_NAME_MAX-1) == 0)) {
        return NULL;
    }

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info == NULL) return NULL;

    dlb_monitor_t *monitor = NULL;
    bool anonymous_region = (name == NULL || *name == '\0');
    bool global_region = !anonymous_region && name == global_region_name;

    /* Check again if the pointers are different but the string content is the
     * same as the global region, ignoring case */
    if (!anonymous_region
            && !global_region
            && strncasecmp(global_region_name, name, DLB_MONITOR_NAME_MAX-1) == 0) {
        name = global_region_name;
        global_region = true;
    }

    /* Found monitor if already registered */
    if (!anonymous_region) {
        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            monitor = g_tree_lookup(talp_info->regions, name);
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);

        if (monitor != NULL) {
            return monitor;
        }
    }

    /* Otherwise, create new monitoring region */
    monitor = malloc(sizeof(dlb_monitor_t));
    fatal_cond(!monitor, "Could not register a new monitoring region."
            " Please report at "PACKAGE_BUGREPORT);

    /* Determine the initial number of assigned CPUs for the region */
    float avg_cpus = CPU_COUNT(&spd->process_mask);

    /* Construct name if anonymous region */
    char monitor_name[DLB_MONITOR_NAME_MAX];
    if (anonymous_region) {
        snprintf(monitor_name, DLB_MONITOR_NAME_MAX, "Anonymous Region %d",
                get_new_anonymous_id());
        name = monitor_name;
    }

    /* Initialize values */
    bool have_shmem = talp_info->flags.have_shmem
        || (talp_info->flags.have_minimal_shmem && global_region);
    region_initialize(monitor, get_new_monitor_id(), name,
            spd->id, avg_cpus, spd->options.talp_region_select, have_shmem);

    /* Finally, insert */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        g_tree_insert(talp_info->regions, (gpointer)monitor->name, monitor);
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);

    return monitor;
}

int region_reset(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == DLB_GLOBAL_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = talp_info->monitor;
    }

    monitor_data_t *monitor_data = monitor->_data;

    /* Close region if started */
    if (monitor_data->flags.started) {

        talp_info_t *talp_info = spd->talp_info;

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            monitor_data->flags.started = false;
            talp_info->open_regions = g_slist_remove(talp_info->open_regions, monitor);
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);

        verbose(VB_TALP, "Stopping region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_END);
    }

    /* Reset everything except these fields: */
    *monitor = (const dlb_monitor_t) {
        .name = monitor->name,
        .num_resets = monitor->num_resets + 1,
        ._data = monitor->_data,
    };

    return DLB_SUCCESS;
}

int region_start(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {

    /* Observer and unknown threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(!thread_is_profiled())) return DLB_ERR_PERM;

    /* Not compatible special value */
    if (unlikely(monitor == DLB_LAST_OPEN_REGION)) return DLB_ERR_NOENT;

    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    }

    /* Specific case: thread-local regions if thread is inside a parallel */
    if (thread_is_parallel()) {
        /* Thread-local regions cannot manage global region */
        if (monitor == talp_info->monitor) return DLB_ERR_PERM;

        return region_start_thread(spd, monitor);
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->flags.started && monitor_data->flags.enabled) {

        /* Update this sample first */
        talp_sample_update(talp_info);

        /* Gather samples from all threads and update regions */
        talp_aggregate_samples_to_regions(talp_info);

        verbose(VB_TALP, "Starting region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_BEGIN);

        /* Thread sample was just updated, use timestamp as starting time */
        talp_sample_t *thread_sample = talp_sample_get(talp_info);
        monitor->start_time = thread_sample->last_updated_ts;
        monitor->stop_time = 0;

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            monitor_data->flags.started = true;
            talp_info->open_regions = g_slist_prepend(talp_info->open_regions, monitor);
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);

        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int region_stop(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {

    /* Observer and unknown threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(!thread_is_profiled())) return DLB_ERR_PERM;

    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    }

    /* Specific case: thread-local regions if thread is inside a parallel */
    if (thread_is_parallel()) {
        /* Thread-local regions cannot manage global region */
        if (monitor == talp_info->monitor) return DLB_ERR_PERM;

        return region_stop_thread(spd, monitor);
    }

    if (monitor == DLB_LAST_OPEN_REGION) {
        if (talp_info->open_regions != NULL) {
            monitor = talp_info->open_regions->data;
        } else {
            return DLB_ERR_NOENT;
        }
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (monitor_data->flags.started) {

        /* Update this sample first */
        talp_sample_update(talp_info);

        /* Gather samples from all threads and update regions */
        talp_aggregate_samples_to_regions(talp_info);

        /* Stop timer */
        talp_sample_t *thread_sample = talp_sample_get(talp_info);
        monitor->stop_time = thread_sample->last_updated_ts;
        monitor->elapsed_time += monitor->stop_time - monitor->start_time;
        ++(monitor->num_measurements);

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            monitor_data->flags.started = false;
            talp_info->open_regions = g_slist_remove(talp_info->open_regions, monitor);
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);

        verbose(VB_TALP, "Stopping region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_END);
        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

bool region_is_started(const dlb_monitor_t *monitor) {
    return ((monitor_data_t*)monitor->_data)->flags.started;
}

void region_set_internal(struct dlb_monitor_t *monitor, bool internal) {
    ((monitor_data_t*)monitor->_data)->flags.internal = internal;
}

int region_report(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor) {
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    }
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return DLB_NOUPDT;
    }

    talp_output_print_monitoring_region(monitor, talp_info->flags);

    return DLB_SUCCESS;
}
