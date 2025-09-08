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

#include "LB_core/DLB_talp.h"

#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_core/talp_types.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/perf_metrics.h"
#include "support/talp_output.h"
#include "LB_comm/shmem_talp.h"
#include "LB_numThreads/omptool.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef PAPI_LIB
#include <papi.h>
#endif

extern __thread bool thread_is_observer;

static inline void set_sample_state(talp_sample_t *sample, enum talp_sample_state state,
        bool papi);
static void talp_dealloc_samples(const subprocess_descriptor_t *spd);
static void talp_record_monitor(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor);
static void talp_aggregate_samples(const subprocess_descriptor_t *spd);
static talp_sample_t* talp_get_thread_sample(const subprocess_descriptor_t *spd);

#if MPI_LIB
static MPI_Datatype mpi_int64_type;
#endif

#if PAPI_LIB
static __thread int EventSet = PAPI_NULL;
#endif

const char *global_region_name = DLB_GLOBAL_REGION_NAME;


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

/* Helper function for GTree: Compare region names */
static gint key_compare_func(gconstpointer a, gconstpointer b) {
    return strncmp(a, b, DLB_MONITOR_NAME_MAX-1);
}

/* Helper function for GTree: deallocate */
static void dealloc_region(gpointer data) {

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
    char *saveptr;
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

static void monitoring_region_initialize(dlb_monitor_t *monitor, int id, const
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

struct dlb_monitor_t* monitoring_region_get_global_region(
        const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    return talp_info ? talp_info->monitor : NULL;
}

const char* monitoring_region_get_global_region_name(void) {
    return global_region_name;
}

dlb_monitor_t* monitoring_region_register(const subprocess_descriptor_t *spd,
        const char* name) {

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
    monitoring_region_initialize(monitor, get_new_monitor_id(), name,
            spd->id, avg_cpus, spd->options.talp_region_select, have_shmem);

    /* Finally, insert */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        g_tree_insert(talp_info->regions, (gpointer)monitor->name, monitor);
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);

    return monitor;
}

int monitoring_region_reset(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == DLB_GLOBAL_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = talp_info->monitor;
    }

    /* Reset everything except these fields: */
    *monitor = (const dlb_monitor_t) {
        .name = monitor->name,
        .num_resets = monitor->num_resets + 1,
        ._data = monitor->_data,
    };

    monitor_data_t *monitor_data = monitor->_data;
    monitor_data->flags.started = false;

    return DLB_SUCCESS;
}

int monitoring_region_start(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    /* Observer threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(thread_is_observer)) return DLB_ERR_PERM;

    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->flags.started && monitor_data->flags.enabled) {
        /* Gather samples from all threads and update regions */
        talp_aggregate_samples(spd);

        verbose(VB_TALP, "Starting region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_BEGIN);

        /* Thread sample was just updated, use timestamp as starting time */
        talp_sample_t *thread_sample = talp_get_thread_sample(spd);
        monitor->start_time = thread_sample->last_updated_timestamp;
        monitor->stop_time = 0;

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            monitor_data->flags.started = true;
            talp_info->open_regions = g_slist_prepend(talp_info->open_regions, monitor);
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);

        /* Normally, the sample state will be 'useful' at this point, but on
         * certain cases where neither talp_mpi_init nor talp_openmp_init have
         * been called, this is necessary */
        if (thread_sample->state != useful) {
            set_sample_state(thread_sample, useful, talp_info->flags.papi);
        }

        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_stop(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    /* Observer threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(thread_is_observer)) return DLB_ERR_PERM;

    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    } else if (monitor == DLB_LAST_OPEN_REGION) {
        if (talp_info->open_regions != NULL) {
            monitor = talp_info->open_regions->data;
        } else {
            return DLB_ERR_NOENT;
        }
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (monitor_data->flags.started) {
        /* Gather samples from all threads and update regions */
        talp_aggregate_samples(spd);

        /* Stop timer */
        talp_sample_t *thread_sample = talp_get_thread_sample(spd);
        monitor->stop_time = thread_sample->last_updated_timestamp;
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

bool monitoring_region_is_started(const dlb_monitor_t *monitor) {
    return ((monitor_data_t*)monitor->_data)->flags.started;
}

void monitoring_region_set_internal(struct dlb_monitor_t *monitor, bool internal) {
    ((monitor_data_t*)monitor->_data)->flags.internal = internal;
}

int monitoring_region_report(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor) {
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_GLOBAL_REGION) {
        monitor = talp_info->monitor;
    }
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return DLB_NOUPDT;
    }

#ifdef PAPI_LIB
    bool have_papi = talp_info->flags.papi;
#else
    bool have_papi = false;
#endif
    talp_output_print_monitoring_region(monitor, mu_to_str(&spd->process_mask),
            talp_info->flags.have_mpi, talp_info->flags.have_openmp, have_papi);

    return DLB_SUCCESS;
}

int monitoring_regions_force_update(const subprocess_descriptor_t *spd) {
    /* Observer threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(thread_is_observer)) return DLB_ERR_PERM;

    talp_aggregate_samples(spd);
    return DLB_SUCCESS;
}

/* Update all monitoring regions with the macrosample */
static void monitoring_regions_update_all(const subprocess_descriptor_t *spd,
        const talp_macrosample_t *macrosample, int num_cpus) {
    talp_info_t *talp_info = spd->talp_info;

    /* Update all open regions */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        for (GSList *node = talp_info->open_regions;
                node != NULL;
                node = node->next) {
            dlb_monitor_t *monitor = node->data;
            monitor_data_t *monitor_data = monitor->_data;

            /* Update number of CPUs if needed */
            monitor->num_cpus = max_int(monitor->num_cpus, num_cpus);

            /* Timers */
            monitor->useful_time += macrosample->timers.useful;
            monitor->mpi_time += macrosample->timers.not_useful_mpi;
            monitor->omp_load_imbalance_time += macrosample->timers.not_useful_omp_in_lb;
            monitor->omp_scheduling_time += macrosample->timers.not_useful_omp_in_sched;
            monitor->omp_serialization_time += macrosample->timers.not_useful_omp_out;
#ifdef PAPI_LIB
            /* Counters */
            monitor->cycles += macrosample->counters.cycles;
            monitor->instructions += macrosample->counters.instructions;
#endif
            /* Stats */
            monitor->num_mpi_calls += macrosample->stats.num_mpi_calls;
            monitor->num_omp_parallels += macrosample->stats.num_omp_parallels;
            monitor->num_omp_tasks += macrosample->stats.num_omp_tasks;

            /* Update shared memory only if requested */
            if (talp_info->flags.external_profiler) {
                shmem_talp__set_times(monitor_data->node_shared_id,
                        monitor->mpi_time,
                        monitor->useful_time);
            }
        }
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}

/* Update all open nested regions (so, excluding the innermost) and add the
 * time since its start time until the sample last timestamp (which is the time
 * that has yet not been added to the regions) as omp_serialization_time */
static void monitoring_regions_update_nested(const subprocess_descriptor_t *spd,
        const talp_sample_t *sample) {

    talp_info_t *talp_info = spd->talp_info;

    /* Update all open nested regions */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        GSList *nested_open_regions = talp_info->open_regions
            ? talp_info->open_regions->next
            : NULL;

        for (GSList *node = nested_open_regions;
                node != NULL;
                node = node->next) {

            dlb_monitor_t *monitor = node->data;
            monitor->omp_serialization_time +=
                sample->last_updated_timestamp - monitor->start_time;
        }
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

/* Executed once */
static inline int init_papi(void) __attribute__((unused));
static inline int init_papi(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    /* Library init */
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        warning("PAPI Library versions differ");
        return -1;
    }

    /* Activate thread tracing */
    int error = PAPI_thread_init(pthread_self);
    if (error != PAPI_OK) {
        warning("PAPI Error during thread initialization. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }
#endif
    return 0;
}

/* Executed once per thread */
static inline int init_papi_counters(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    int error = PAPI_register_thread();
    if (error != PAPI_OK) {
        warning("PAPI Error during thread registration. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    /* Eventset creation */
    EventSet = PAPI_NULL;
    error = PAPI_create_eventset(&EventSet);
    if (error != PAPI_OK) {
        warning("PAPI Error during eventset creation. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    int Events[2] = {PAPI_TOT_CYC, PAPI_TOT_INS};
    error = PAPI_add_events(EventSet, Events, 2);
    if (error != PAPI_OK) {
        warning("PAPI Error adding events. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    /* Start tracing  */
    error = PAPI_start(EventSet);
    if (error != PAPI_OK) {
        warning("PAPI Error during tracing initialization: %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }
#endif
    return 0;
}

static inline void reset_papi_counters(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    int error = PAPI_reset(EventSet);
    if (error != PAPI_OK) verbose(VB_TALP, "Error resetting counters");
#endif
}

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .flags = {
            .external_profiler = spd->options.talp_external_profiler,
            .papi = spd->options.talp_papi,
            .have_shmem = spd->options.talp_external_profiler,
            .have_minimal_shmem = !spd->options.talp_external_profiler
                && spd->options.talp_summary & SUMMARY_NODE,
        },
        .regions = g_tree_new_full(
                (GCompareDataFunc)key_compare_func,
                NULL, NULL, dealloc_region),
        .regions_mutex = PTHREAD_MUTEX_INITIALIZER,
        .samples_mutex = PTHREAD_MUTEX_INITIALIZER,
    };
    spd->talp_info = talp_info;

    /* Initialize shared memory */
    if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
        /* If we only need a minimal shmem, its size will be the user-provided
         * multiplier times 'system_size' (usually, 1 region per process)
         * Otherwise, we multiply it by DEFAULT_REGIONS_PER_PROC.
         */
        enum { DEFAULT_REGIONS_PER_PROC = 100 };
        int shmem_size_multiplier = spd->options.shm_size_multiplier
            * (talp_info->flags.have_shmem ? DEFAULT_REGIONS_PER_PROC : 1);
        shmem_talp__init(spd->options.shm_key, shmem_size_multiplier);
    }

    /* Initialize global region monitor
     * (at this point we don't know how many CPUs, it will be fixed in talp_openmp_init) */
    talp_info->monitor = monitoring_region_register(spd, global_region_name);

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&spd->process_mask));

    /* Initialize and start running PAPI */
    if (talp_info->flags.papi) {
#ifdef PAPI_LIB
        if (init_papi() != 0 || init_papi_counters() != 0) {
            warning("PAPI initialization has failed, disabling option.");
            talp_info->flags.papi = false;
        }
#else
        warning("DLB has not been configured with PAPI support, disabling option.");
        talp_info->flags.papi = false;
#endif
    }
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_finalize");
    verbose(VB_TALP, "Finalizing TALP module");

    talp_info_t *talp_info = spd->talp_info;
    if (!talp_info->flags.have_mpi) {
        /* If we don't have MPI support, regions may be still running and
         * without being recorded to talp_output. Do that now. */

        /* Stop open regions
         * (Note that monitoring_region_stop need to acquire the regions_mutex
         * lock, so we we need to iterate without it) */
        while(talp_info->open_regions != NULL) {
            dlb_monitor_t *monitor = talp_info->open_regions->data;
            monitoring_region_stop(spd, monitor);
        }

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            /* Record all regions */
            for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                    node != NULL;
                    node = g_tree_node_next(node)) {
                const dlb_monitor_t *monitor = g_tree_node_value(node);
                talp_record_monitor(spd, monitor);
            }
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);
    }

    /* Print/write all collected summaries */
    talp_output_finalize(spd->options.talp_output_file);

    /* Deallocate samples structure */
    talp_dealloc_samples(spd);

    /* Finalize shared memory */
    if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
        shmem_talp__finalize(spd->id);
    }

#ifdef PAPI_LIB
    if (talp_info->flags.papi) {
        PAPI_shutdown();
    }
#endif

    /* Deallocate monitoring regions and talp_info */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        /* Destroy GTree, each node is deallocated with the function dealloc_region */
        g_tree_destroy(talp_info->regions);
        talp_info->regions = NULL;
        talp_info->monitor = NULL;

        /* Destroy list of open regions */
        g_slist_free(talp_info->open_regions);
        talp_info->open_regions = NULL;
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
    free(talp_info);
    spd->talp_info = NULL;
}


/*********************************************************************************/
/*    Sample functions                                                           */
/*********************************************************************************/

static __thread talp_sample_t* _tls_sample = NULL;

/* WARNING: this function may only be called when updating own thread's sample */
static inline void set_sample_state(talp_sample_t *sample, enum talp_sample_state state,
        bool papi) {
    sample->state = state;
    if (papi && state == useful) {
        reset_papi_counters();
    }
    instrument_event(MONITOR_STATE,
            state == disabled ? MONITOR_STATE_DISABLED
            : state == useful ? MONITOR_STATE_USEFUL
            : state == not_useful_mpi ? MONITOR_STATE_NOT_USEFUL_MPI
            : state == not_useful_omp_in ? MONITOR_STATE_NOT_USEFUL_OMP_IN
            : state == not_useful_omp_out ? MONITOR_STATE_NOT_USEFUL_OMP_OUT
            : 0,
            EVENT_BEGIN);
}

/* Get the TLS associated sample */
static talp_sample_t* talp_get_thread_sample(const subprocess_descriptor_t *spd) {
    /* Thread already has an allocated sample, return it */
    if (likely(_tls_sample != NULL)) return _tls_sample;

    /* Observer threads don't have a valid sample */
    if (unlikely(thread_is_observer)) return NULL;

    /* Otherwise, allocate */
    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        int ncpus = ++talp_info->ncpus;
        void *samples = realloc(talp_info->samples, sizeof(talp_sample_t*)*ncpus);
        if (samples) {
            talp_info->samples = samples;
            void *new_sample;
            if (posix_memalign(&new_sample, DLB_CACHE_LINE, sizeof(talp_sample_t)) == 0) {
                _tls_sample = new_sample;
                talp_info->samples[ncpus-1] = new_sample;
            }
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    fatal_cond(_tls_sample == NULL, "TALP: could not allocate thread sample");

    /* If a thread is created mid-region, its initial time is that of the
     * innermost open region, otherwise it is the current time */
    int64_t last_updated_timestamp;
    if (talp_info->open_regions) {
        const dlb_monitor_t *monitor = talp_info->open_regions->data;
        last_updated_timestamp = monitor->start_time;
    } else {
        last_updated_timestamp = get_time_in_ns();
    }

    *_tls_sample = (const talp_sample_t) {
        .last_updated_timestamp = last_updated_timestamp,
    };

    set_sample_state(_tls_sample, disabled, talp_info->flags.papi);

#ifdef INSTRUMENTATION_VERSION
    unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
    long long papi_values[] = {0, 0};
    instrument_nevent(2, events, papi_values);
#endif

    return _tls_sample;
}

static void talp_dealloc_samples(const subprocess_descriptor_t *spd) {
    /* This only nullifies the current thread pointer, but this function is
     * only really important when TALP is finalized and then initialized again
     */
    _tls_sample = NULL;

    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        free(talp_info->samples);
        talp_info->samples = NULL;
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);
}

enum { NO_TIMESTAMP = 0 };
/* Compute new microsample (time since last update) and update sample values */
static void talp_update_sample(talp_sample_t *sample, bool papi, int64_t timestamp) {
    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_timestamp */
    int64_t now = timestamp == NO_TIMESTAMP ? get_time_in_ns() : timestamp;
    int64_t microsample_duration = now - sample->last_updated_timestamp;
    sample->last_updated_timestamp = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case disabled:
            break;
        case useful:
            DLB_ATOMIC_ADD_RLX(&sample->timers.useful, microsample_duration);
            break;
        case not_useful_mpi:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_mpi, microsample_duration);
            break;
        case not_useful_omp_in:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_in, microsample_duration);
            break;
        case not_useful_omp_out:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_out, microsample_duration);
            break;
    }

#ifdef PAPI_LIB
    if (papi) {
        /* Only read counters if we are updating this thread's sample */
        if (sample == talp_get_thread_sample(thread_spd)) {
            if (sample->state == useful) {
                /* Read */
                long long papi_values[2];
                int error = PAPI_read(EventSet, papi_values);
                if (error != PAPI_OK) {
                    verbose(VB_TALP, "stop return code: %d, %s", error, PAPI_strerror(error));
                }

                /* Atomically add papi_values to sample structure */
                DLB_ATOMIC_ADD_RLX(&sample->counters.cycles, papi_values[0]);
                DLB_ATOMIC_ADD_RLX(&sample->counters.instructions, papi_values[1]);

#ifdef INSTRUMENTATION_VERSION
                unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
                instrument_nevent(2, events, papi_values);
#endif

                /* Counters are reset here and each time the sample is set to useful */
                reset_papi_counters();
            }
            else {
#ifdef INSTRUMENTATION_VERSION
                /* Emit 0's to distinguish useful chunks in traces */
                unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
                long long papi_values[] = {0, 0};
                instrument_nevent(2, events, papi_values);
#endif
            }
        }
    }
#endif
}

/* Flush and aggregate a single sample into a macrosample */
static inline void talp_aggregate_sample(talp_sample_t *sample,
        talp_macrosample_t *macrosample) {
    /* Timers */
    macrosample->timers.useful +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.useful, 0);
    macrosample->timers.not_useful_mpi +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_mpi, 0);
    macrosample->timers.not_useful_omp_out +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_omp_out, 0);
    /* timers.not_useful_omp_in is not flushed here, make sure struct is empty */
    ensure(DLB_ATOMIC_LD_RLX(&sample->timers.not_useful_omp_in) == 0,
                "Inconsistency in TALP sample metric not_useful_omp_in."
                " Please, report bug at " PACKAGE_BUGREPORT);

#ifdef PAPI_LIB
    /* Counters */
    macrosample->counters.cycles +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.cycles, 0);
    macrosample->counters.instructions +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.instructions, 0);
#endif

    /* Stats */
    macrosample->stats.num_mpi_calls +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_mpi_calls, 0);
    macrosample->stats.num_omp_parallels +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_omp_parallels, 0);
    macrosample->stats.num_omp_tasks +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_omp_tasks, 0);
}

/* Accumulate values from samples of all threads and update regions */
static void talp_aggregate_samples(const subprocess_descriptor_t *spd) {

    int num_cpus;
    talp_info_t *talp_info = spd->talp_info;

    /* Accumulate samples from all threads */
    talp_macrosample_t macrosample = (const talp_macrosample_t) {};
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        num_cpus = talp_info->ncpus;

        /* Force-update and aggregate all samples */
        int64_t timestamp = get_time_in_ns();
        for (int i = 0; i < num_cpus; ++i) {
            talp_update_sample(talp_info->samples[i], talp_info->flags.papi, timestamp);
            talp_aggregate_sample(talp_info->samples[i], &macrosample);
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    /* Update all started regions */
    monitoring_regions_update_all(spd, &macrosample, num_cpus);
}

/* Accumulate samples from only a subset of samples of a parallel region.
 * Load Balance and Scheduling are computed here based on all samples. */
static void talp_aggregate_samples_parallel(const subprocess_descriptor_t *spd,
        talp_sample_t **samples, unsigned int nelems) {

    talp_info_t *talp_info = spd->talp_info;
    talp_macrosample_t macrosample = (const talp_macrosample_t) {};
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        /* Iterate first to force-update all samples and compute the minimum
         * not-useful-omp-in among them */
        int64_t timestamp = get_time_in_ns();
        int64_t min_not_useful_omp_in = INT64_MAX;
        unsigned int i;
        for (i=0; i<nelems; ++i) {
            talp_update_sample(samples[i], talp_info->flags.papi, timestamp);
            min_not_useful_omp_in = min_int64(min_not_useful_omp_in,
                    DLB_ATOMIC_LD_RLX(&samples[i]->timers.not_useful_omp_in));
        }

        /* Iterate again to accumulate Load Balance, and to aggregate sample */
        int64_t sched_timer = min_not_useful_omp_in * nelems;
        int64_t lb_timer = 0;
        for (i=0; i<nelems; ++i) {
            lb_timer += DLB_ATOMIC_EXCH_RLX(&samples[i]->timers.not_useful_omp_in, 0)
                - min_not_useful_omp_in;
            talp_aggregate_sample(samples[i], &macrosample);
        }

        /* Update derived timers into macrosample */
        macrosample.timers.not_useful_omp_in_lb = lb_timer;
        macrosample.timers.not_useful_omp_in_sched = sched_timer;
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    /* Update all started regions */
    monitoring_regions_update_all(spd, &macrosample, nelems);
}


/*********************************************************************************/
/*    TALP Record in serial (non-MPI) mode                                       */
/*********************************************************************************/

/* For any given monitor, record metrics considering only this (sub-)process */
static void talp_record_monitor(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {
    if (spd->options.talp_summary & SUMMARY_PROCESS) {
        verbose(VB_TALP, "TALP process summary: recording region %s", monitor->name);

        process_record_t process_record = {
            .rank = 0,
            .pid = spd->id,
            .monitor = *monitor,
        };

        /* Fill hostname and CPU mask strings in process_record */
        gethostname(process_record.hostname, HOST_NAME_MAX);
        snprintf(process_record.cpuset, TALP_OUTPUT_CPUSET_MAX, "%s",
                mu_to_str(&spd->process_mask));
        mu_get_quoted_mask(&spd->process_mask, process_record.cpuset_quoted,
            TALP_OUTPUT_CPUSET_MAX);

        /* Add record */
        talp_output_record_process(monitor->name, &process_record, 1);
    }

    if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
        if (monitor->elapsed_time > 0) {
            verbose(VB_TALP, "TALP summary: recording region %s", monitor->name);

            talp_info_t *talp_info = spd->talp_info;
            const pop_base_metrics_t base_metrics = {
                .num_cpus                = monitor->num_cpus,
                .num_mpi_ranks           = 0,
                .num_nodes               = 1,
                .avg_cpus                = monitor->avg_cpus,
                .cycles                  = (double)monitor->cycles,
                .instructions            = (double)monitor->instructions,
                .num_measurements        = monitor->num_measurements,
                .num_mpi_calls           = monitor->num_mpi_calls,
                .num_omp_parallels       = monitor->num_omp_parallels,
                .num_omp_tasks           = monitor->num_omp_tasks,
                .elapsed_time            = monitor->elapsed_time,
                .useful_time             = monitor->useful_time,
                .mpi_time                = monitor->mpi_time,
                .omp_load_imbalance_time = monitor->omp_load_imbalance_time,
                .omp_scheduling_time     = monitor->omp_scheduling_time,
                .omp_serialization_time  = monitor->omp_serialization_time,
                .useful_normd_app        = (double)monitor->useful_time / monitor->num_cpus,
                .mpi_normd_app           = (double)monitor->mpi_time / monitor->num_cpus,
                .max_useful_normd_proc   = (double)monitor->useful_time / monitor->num_cpus,
                .max_useful_normd_node   = (double)monitor->useful_time / monitor->num_cpus,
                .mpi_normd_of_max_useful = (double)monitor->mpi_time / monitor->num_cpus,
            };

            dlb_pop_metrics_t pop_metrics;
            talp_base_metrics_to_pop_metrics(monitor->name, &base_metrics, &pop_metrics);
            talp_output_record_pop_metrics(&pop_metrics);

            if(monitor == talp_info->monitor) {
                talp_output_record_resources(monitor->num_cpus,
                        /* num_nodes */ 1, /* num_ranks */ 0);
            }

        } else {
            verbose(VB_TALP, "TALP summary: recording empty region %s", monitor->name);
            dlb_pop_metrics_t pop_metrics = {0};
            snprintf(pop_metrics.name, DLB_MONITOR_NAME_MAX, "%s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);
        }
    }
}


/*********************************************************************************/
/*    TALP Record in MPI mode                                                    */
/*********************************************************************************/

#if MPI_LIB
/* Compute Node summary of all Global Monitors and record data */
static void talp_record_node_summary(const subprocess_descriptor_t *spd) {

    node_record_t *node_summary = NULL;
    size_t node_summary_size = 0;

    /* Perform a barrier so that all processes in the node have arrived at the
     * MPI_Finalize */
    node_barrier(spd, NULL);

    /* Node process 0 reduces all global regions from all processes in the node */
    if (_process_id == 0) {
        /* Obtain a list of regions associated with the Global Region Name, sorted by PID */
        int max_procs = mu_get_system_size();
        talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
        int nelems;
        shmem_talp__get_regionlist(region_list, &nelems, max_procs, global_region_name);

        /* Allocate and initialize node summary structure */
        node_summary_size = sizeof(node_record_t) + sizeof(process_in_node_record_t) * nelems;
        node_summary = malloc(node_summary_size);
        *node_summary = (const node_record_t) {
            .node_id = _node_id,
            .nelems = nelems,
        };

        /* Iterate the PID list and gather times of every process */
        for (int i = 0; i < nelems; ++i) {
            int64_t mpi_time = region_list[i].mpi_time;
            int64_t useful_time = region_list[i].useful_time;

            /* Save times in local structure */
            node_summary->processes[i].pid = region_list[i].pid;
            node_summary->processes[i].mpi_time = mpi_time;
            node_summary->processes[i].useful_time = useful_time;

            /* Accumulate total and max values */
            node_summary->avg_useful_time += useful_time;
            node_summary->avg_mpi_time += mpi_time;
            node_summary->max_useful_time = max_int64(useful_time, node_summary->max_useful_time);
            node_summary->max_mpi_time = max_int64(mpi_time, node_summary->max_mpi_time);
        }
        free(region_list);

        /* Compute average values */
        node_summary->avg_useful_time /= node_summary->nelems;
        node_summary->avg_mpi_time /= node_summary->nelems;
    }

    /* Perform a final barrier so that all processes let the _process_id 0 to
     * gather all the data */
    node_barrier(spd, NULL);

    /* All main processes from each node send data to rank 0 */
    if (_process_id == 0) {
        verbose(VB_TALP, "Node summary: gathering data");

        /* MPI type: pid_t */
        MPI_Datatype mpi_pid_type;
        PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(pid_t), &mpi_pid_type);

        /* MPI struct type: process_in_node_record_t */
        MPI_Datatype mpi_process_info_type;
        {
            int count = 3;
            int blocklengths[] = {1, 1, 1};
            MPI_Aint displacements[] = {
                offsetof(process_in_node_record_t, pid),
                offsetof(process_in_node_record_t, mpi_time),
                offsetof(process_in_node_record_t, useful_time)};
            MPI_Datatype types[] = {mpi_pid_type, mpi_int64_type, mpi_int64_type};
            MPI_Datatype tmp_type;
            PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
            PMPI_Type_create_resized(tmp_type, 0, sizeof(process_in_node_record_t),
                    &mpi_process_info_type);
            PMPI_Type_commit(&mpi_process_info_type);
        }

        /* MPI struct type: node_record_t */
        MPI_Datatype mpi_node_record_type;;
        {
            int count = 7;
            int blocklengths[] = {1, 1, 1, 1, 1, 1, node_summary->nelems};
            MPI_Aint displacements[] = {
                offsetof(node_record_t, node_id),
                offsetof(node_record_t, nelems),
                offsetof(node_record_t, avg_useful_time),
                offsetof(node_record_t, avg_mpi_time),
                offsetof(node_record_t, max_useful_time),
                offsetof(node_record_t, max_mpi_time),
                offsetof(node_record_t, processes)};
            MPI_Datatype types[] = {MPI_INT, MPI_INT, mpi_int64_type, mpi_int64_type,
                mpi_int64_type, mpi_int64_type, mpi_process_info_type};
            MPI_Datatype tmp_type;
            PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
            PMPI_Type_create_resized(tmp_type, 0, node_summary_size, &mpi_node_record_type);
            PMPI_Type_commit(&mpi_node_record_type);
        }

        /* Gather data */
        void *recvbuf = NULL;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_num_nodes * node_summary_size);
        }
        PMPI_Gather(node_summary, 1, mpi_node_record_type,
                recvbuf, 1, mpi_node_record_type,
                0, getInterNodeComm());

        /* Free send buffer and MPI Datatypes */
        free(node_summary);
        PMPI_Type_free(&mpi_process_info_type);
        PMPI_Type_free(&mpi_node_record_type);

        /* Add records */
        if (_mpi_rank == 0) {
            for (int node_id = 0; node_id < _num_nodes; ++node_id) {
                verbose(VB_TALP, "Node summary: recording node %d", node_id);
                node_record_t *node_record = (node_record_t*)(
                        (unsigned char *)recvbuf + node_summary_size * node_id);
                ensure( node_id == node_record->node_id, "Node id error in %s", __func__ );
                talp_output_record_node(node_record);
            }
            free(recvbuf);
        }
    }
}
#endif

#ifdef MPI_LIB
/* Gather PROCESS data of a monitor among all ranks and record it in rank 0 */
static void talp_record_process_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {

    /* Internal monitors will not be recorded */
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return;
    }

    if (_mpi_rank == 0) {
        verbose(VB_TALP, "Process summary: gathering region %s", monitor->name);
    }

    process_record_t process_record_send = {
        .rank = _mpi_rank,
        .pid = spd->id,
        .node_id = _node_id,
        .monitor = *monitor,
    };

    /* Invalidate pointers of the copied monitor */
    process_record_send.monitor.name = NULL;
    process_record_send.monitor._data = NULL;

    /* Fill hostname and CPU mask strings in process_record_send */
    gethostname(process_record_send.hostname, HOST_NAME_MAX);
    snprintf(process_record_send.cpuset, TALP_OUTPUT_CPUSET_MAX, "%s",
            mu_to_str(&spd->process_mask));
    mu_get_quoted_mask(&spd->process_mask, process_record_send.cpuset_quoted,
            TALP_OUTPUT_CPUSET_MAX);

    /* MPI type: pid_t */
    MPI_Datatype mpi_pid_type;
    PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(pid_t), &mpi_pid_type);

    /* Note: obviously, it doesn't make sense to send addresses via MPI, but we
     * are sending the whole dlb_monitor_t, so... Addresses are discarded
     * either way. */

    /* MPI type: void* */
    MPI_Datatype address_type;
    PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(void*), &address_type);

    /* MPI struct type: dlb_monitor_t */
    MPI_Datatype mpi_dlb_monitor_type;
    {
        enum {count = 19};
        int blocklengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        MPI_Aint displacements[] = {
            offsetof(dlb_monitor_t, name),
            offsetof(dlb_monitor_t, num_cpus),
            offsetof(dlb_monitor_t, avg_cpus),
            offsetof(dlb_monitor_t, cycles),
            offsetof(dlb_monitor_t, instructions),
            offsetof(dlb_monitor_t, num_measurements),
            offsetof(dlb_monitor_t, num_resets),
            offsetof(dlb_monitor_t, num_mpi_calls),
            offsetof(dlb_monitor_t, num_omp_parallels),
            offsetof(dlb_monitor_t, num_omp_tasks),
            offsetof(dlb_monitor_t, start_time),
            offsetof(dlb_monitor_t, stop_time),
            offsetof(dlb_monitor_t, elapsed_time),
            offsetof(dlb_monitor_t, useful_time),
            offsetof(dlb_monitor_t, mpi_time),
            offsetof(dlb_monitor_t, omp_load_imbalance_time),
            offsetof(dlb_monitor_t, omp_scheduling_time),
            offsetof(dlb_monitor_t, omp_serialization_time),
            offsetof(dlb_monitor_t, _data)};
        MPI_Datatype types[] = {address_type, MPI_INT, MPI_FLOAT,
            mpi_int64_type, mpi_int64_type, MPI_INT, MPI_INT, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, address_type};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(dlb_monitor_t), &mpi_dlb_monitor_type);
        PMPI_Type_commit(&mpi_dlb_monitor_type);

        static_ensure(sizeof(blocklengths)/sizeof(blocklengths[0]) == count);
        static_ensure(sizeof(displacements)/sizeof(displacements[0]) == count);
        static_ensure(sizeof(types)/sizeof(types[0]) == count);
    }

    /* MPI struct type: process_record_t */
    MPI_Datatype mpi_process_record_type;
    {
        int count = 7;
        int blocklengths[] = {1, 1, 1, HOST_NAME_MAX,
            TALP_OUTPUT_CPUSET_MAX, TALP_OUTPUT_CPUSET_MAX, 1};
        MPI_Aint displacements[] = {
            offsetof(process_record_t, rank),
            offsetof(process_record_t, pid),
            offsetof(process_record_t, node_id),
            offsetof(process_record_t, hostname),
            offsetof(process_record_t, cpuset),
            offsetof(process_record_t, cpuset_quoted),
            offsetof(process_record_t, monitor)};
        MPI_Datatype types[] = {MPI_INT, mpi_pid_type, MPI_INT, MPI_CHAR, MPI_CHAR,
            MPI_CHAR, mpi_dlb_monitor_type};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(process_record_t),
                &mpi_process_record_type);
        PMPI_Type_commit(&mpi_process_record_type);
    }

    /* Gather data */
    process_record_t *recvbuf = NULL;
    if (_mpi_rank == 0) {
        recvbuf = malloc(_mpi_size * sizeof(process_record_t));
    }
    PMPI_Gather(&process_record_send, 1, mpi_process_record_type,
            recvbuf, 1, mpi_process_record_type,
            0, getWorldComm());

    /* Add records */
    if (_mpi_rank == 0) {
        for (int rank = 0; rank < _mpi_size; ++rank) {
            verbose(VB_TALP, "Process summary: recording region %s on rank %d",
                    monitor->name, rank);
            talp_output_record_process(monitor->name, &recvbuf[rank], _mpi_size);
        }
        free(recvbuf);
    }

    /* Free MPI types */
    PMPI_Type_free(&mpi_dlb_monitor_type);
    PMPI_Type_free(&mpi_process_record_type);
}
#endif

#ifdef MPI_LIB

/* The following node and app reductions are needed to compute POP metrics: */

/*** Node reduction ***/

/* Data type to reduce among processes in node */
typedef struct node_reduction {
    bool node_used;
    int cpus_node;
    int64_t sum_useful;
} node_reduction_t;

/* Function called in the MPI node reduction */
static void talp_mpi_node_reduction_fn(void *invec, void *inoutvec, int *len,
        MPI_Datatype *datatype) {
    node_reduction_t *in = invec;
    node_reduction_t *inout = inoutvec;

    int _len = *len;
    for (int i = 0; i < _len; ++i) {
        if (in[i].node_used) {
            inout[i].node_used = true;
            inout[i].cpus_node += in[i].cpus_node;
            inout[i].sum_useful += in[i].sum_useful;
        }
    }
}

/* Function to perform the reduction at node level */
static void talp_reduce_pop_metrics_node_reduction(node_reduction_t *node_reduction,
        const dlb_monitor_t *monitor) {

    const node_reduction_t node_reduction_send = {
        .node_used = monitor->num_measurements > 0,
        .cpus_node = monitor->num_cpus,
        .sum_useful = monitor->useful_time,
    };

    /* MPI struct type: node_reduction_t */
    MPI_Datatype mpi_node_reduction_type;
    {
        int count = 3;
        int blocklengths[] = {1, 1, 1};
        MPI_Aint displacements[] = {
            offsetof(node_reduction_t, node_used),
            offsetof(node_reduction_t, cpus_node),
            offsetof(node_reduction_t, sum_useful)};
        MPI_Datatype types[] = {MPI_C_BOOL, MPI_INT, mpi_int64_type};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(node_reduction_t),
                &mpi_node_reduction_type);
        PMPI_Type_commit(&mpi_node_reduction_type);
    }

    /* Define MPI operation */
    MPI_Op node_reduction_op;
    PMPI_Op_create(talp_mpi_node_reduction_fn, true, &node_reduction_op);

    /* MPI reduction */
    PMPI_Reduce(&node_reduction_send, node_reduction, 1,
            mpi_node_reduction_type, node_reduction_op,
            0, getNodeComm());

    /* Free MPI types */
    PMPI_Type_free(&mpi_node_reduction_type);
    PMPI_Op_free(&node_reduction_op);
}

/** App reduction ***/

/* Data type to reduce among processes in application */
typedef struct app_reduction_t {
    int num_cpus;
    int num_nodes;
    float avg_cpus;
    double cycles;
    double instructions;
    int64_t num_measurements;
    int64_t num_mpi_calls;
    int64_t num_omp_parallels;
    int64_t num_omp_tasks;
    int64_t elapsed_time;
    int64_t useful_time;
    int64_t mpi_time;
    int64_t omp_load_imbalance_time;
    int64_t omp_scheduling_time;
    int64_t omp_serialization_time;
    double  max_useful_normd_proc;
    double  max_useful_normd_node;
    double  mpi_normd_of_max_useful;
} app_reduction_t;

/* Function called in the MPI app reduction */
static void talp_mpi_reduction_fn(void *invec, void *inoutvec, int *len,
        MPI_Datatype *datatype) {
    app_reduction_t *in = invec;
    app_reduction_t *inout = inoutvec;

    int _len = *len;
    for (int i = 0; i < _len; ++i) {
        inout[i].num_cpus                += in[i].num_cpus;
        inout[i].num_nodes               += in[i].num_nodes;
        inout[i].avg_cpus                += in[i].avg_cpus;
        inout[i].cycles                  += in[i].cycles;
        inout[i].instructions            += in[i].instructions;
        inout[i].num_measurements        += in[i].num_measurements;
        inout[i].num_mpi_calls           += in[i].num_mpi_calls;
        inout[i].num_omp_parallels       += in[i].num_omp_parallels;
        inout[i].num_omp_tasks           += in[i].num_omp_tasks;
        inout[i].elapsed_time             = max_int64(inout[i].elapsed_time, in[i].elapsed_time);
        inout[i].useful_time             += in[i].useful_time;
        inout[i].mpi_time                += in[i].mpi_time;
        inout[i].omp_load_imbalance_time += in[i].omp_load_imbalance_time;
        inout[i].omp_scheduling_time     += in[i].omp_scheduling_time;
        inout[i].omp_serialization_time  += in[i].omp_serialization_time;
        inout[i].max_useful_normd_node  =
            max_int64(inout[i].max_useful_normd_node, in[i].max_useful_normd_node);
        if (in[i].max_useful_normd_proc > inout[i].max_useful_normd_proc) {
            inout[i].max_useful_normd_proc = in[i].max_useful_normd_proc;
            inout[i].mpi_normd_of_max_useful = in[i].mpi_normd_of_max_useful;
        }
    }
}

/* Function to perform the reduction at application level */
static void talp_reduce_pop_metrics_app_reduction(app_reduction_t *app_reduction,
        const node_reduction_t *node_reduction, const dlb_monitor_t *monitor,
        bool all_to_all) {

    double max_useful_normd_proc = monitor->num_cpus == 0 ? 0.0
        : (double)monitor->useful_time / monitor->num_cpus;
    double max_useful_normd_node = _process_id != 0 ? 0.0
        : node_reduction->cpus_node == 0 ? 0.0
        : (double)node_reduction->sum_useful / node_reduction->cpus_node;
    double mpi_normd_of_max_useful = monitor->num_cpus == 0 ? 0.0
        : (double)monitor->mpi_time / monitor->num_cpus;

    const app_reduction_t app_reduction_send = {
        .num_cpus                = monitor->num_cpus,
        .num_nodes               = _process_id == 0 && node_reduction->node_used ? 1 : 0,
        .avg_cpus                = monitor->avg_cpus,
        .cycles                  = (double)monitor->cycles,
        .instructions            = (double)monitor->instructions,
        .num_measurements        = monitor->num_measurements,
        .num_mpi_calls           = monitor->num_mpi_calls,
        .num_omp_parallels       = monitor->num_omp_parallels,
        .num_omp_tasks           = monitor->num_omp_tasks,
        .elapsed_time            = monitor->elapsed_time,
        .useful_time             = monitor->useful_time,
        .mpi_time                = monitor->mpi_time,
        .omp_load_imbalance_time = monitor->omp_load_imbalance_time,
        .omp_scheduling_time     = monitor->omp_scheduling_time,
        .omp_serialization_time  = monitor->omp_serialization_time,
        .max_useful_normd_proc   = max_useful_normd_proc,
        .max_useful_normd_node   = max_useful_normd_node,
        .mpi_normd_of_max_useful = mpi_normd_of_max_useful,
    };

    /* MPI struct type: app_reduction_t */
    MPI_Datatype mpi_app_reduction_type;
    {
        enum {count = 18};
        int blocklengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        MPI_Aint displacements[] = {
            offsetof(app_reduction_t, num_cpus),
            offsetof(app_reduction_t, num_nodes),
            offsetof(app_reduction_t, avg_cpus),
            offsetof(app_reduction_t, cycles),
            offsetof(app_reduction_t, instructions),
            offsetof(app_reduction_t, num_measurements),
            offsetof(app_reduction_t, num_mpi_calls),
            offsetof(app_reduction_t, num_omp_parallels),
            offsetof(app_reduction_t, num_omp_tasks),
            offsetof(app_reduction_t, elapsed_time),
            offsetof(app_reduction_t, useful_time),
            offsetof(app_reduction_t, mpi_time),
            offsetof(app_reduction_t, omp_load_imbalance_time),
            offsetof(app_reduction_t, omp_scheduling_time),
            offsetof(app_reduction_t, omp_serialization_time),
            offsetof(app_reduction_t, max_useful_normd_proc),
            offsetof(app_reduction_t, max_useful_normd_node),
            offsetof(app_reduction_t, mpi_normd_of_max_useful)};
        MPI_Datatype types[] = {MPI_INT, MPI_INT, MPI_FLOAT, MPI_DOUBLE,
            MPI_DOUBLE, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, MPI_DOUBLE,
            MPI_DOUBLE, MPI_DOUBLE};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(app_reduction_t),
                &mpi_app_reduction_type);
        PMPI_Type_commit(&mpi_app_reduction_type);

        static_ensure(sizeof(blocklengths)/sizeof(blocklengths[0]) == count);
        static_ensure(sizeof(displacements)/sizeof(displacements[0]) == count);
        static_ensure(sizeof(types)/sizeof(types[0]) == count);
    }

    /* Define MPI operation */
    MPI_Op app_reduction_op;
    MPI_Op_create(talp_mpi_reduction_fn, true, &app_reduction_op);

    /* MPI reduction */
    if (!all_to_all) {
        PMPI_Reduce(&app_reduction_send, app_reduction, 1,
                mpi_app_reduction_type, app_reduction_op,
                0, getWorldComm());
    } else {
        PMPI_Allreduce(&app_reduction_send, app_reduction, 1,
                mpi_app_reduction_type, app_reduction_op,
                getWorldComm());
    }

    /* Free MPI types */
    PMPI_Type_free(&mpi_app_reduction_type);
    PMPI_Op_free(&app_reduction_op);
}

static void talp_reduce_pop_metrics(pop_base_metrics_t *base_metrics,
        const dlb_monitor_t *monitor, bool all_to_all) {

    /* First, reduce some values among processes in the node,
     * needed to compute pop metrics */
    node_reduction_t node_reduction = {0};
    talp_reduce_pop_metrics_node_reduction(&node_reduction, monitor);

    /* With the node reduction, reduce again among all process */
    app_reduction_t app_reduction = {0};
    talp_reduce_pop_metrics_app_reduction(&app_reduction, &node_reduction,
            monitor, all_to_all);

    /* Finally, fill output base_metrics... */

    int num_mpi_ranks;
    PMPI_Comm_size(getWorldComm(), &num_mpi_ranks);

    /* These values do not need a specific MPI reduction and can be deduced
     * from the already reduced number of CPUs */
    double useful_normd_app = app_reduction.num_cpus == 0 ? 0.0
        : (double)app_reduction.useful_time / app_reduction.num_cpus;
    double mpi_normd_app = app_reduction.num_cpus == 0 ? 0.0
        : (double)app_reduction.mpi_time / app_reduction.num_cpus;

    *base_metrics = (const pop_base_metrics_t) {
        .num_cpus                = app_reduction.num_cpus,
        .num_mpi_ranks           = num_mpi_ranks,
        .num_nodes               = app_reduction.num_nodes,
        .avg_cpus                = app_reduction.avg_cpus,
        .cycles                  = app_reduction.cycles,
        .instructions            = app_reduction.instructions,
        .num_measurements        = app_reduction.num_measurements,
        .num_mpi_calls           = app_reduction.num_mpi_calls,
        .num_omp_parallels       = app_reduction.num_omp_parallels,
        .num_omp_tasks           = app_reduction.num_omp_tasks,
        .elapsed_time            = app_reduction.elapsed_time,
        .useful_time             = app_reduction.useful_time,
        .mpi_time                = app_reduction.mpi_time,
        .omp_load_imbalance_time = app_reduction.omp_load_imbalance_time,
        .omp_scheduling_time     = app_reduction.omp_scheduling_time,
        .omp_serialization_time  = app_reduction.omp_serialization_time,
        .useful_normd_app        = useful_normd_app,
        .mpi_normd_app           = mpi_normd_app,
        .max_useful_normd_proc   = app_reduction.max_useful_normd_proc,
        .max_useful_normd_node   = app_reduction.max_useful_normd_node,
        .mpi_normd_of_max_useful = app_reduction.mpi_normd_of_max_useful,
    };
}
#endif

#ifdef MPI_LIB
/* Gather POP METRICS data of a monitor among all ranks and record it in rank 0 */
static void talp_record_pop_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {

    /* Internal monitors will not be recorded */
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return;
    }

    if (_mpi_rank == 0) {
        verbose(VB_TALP, "TALP summary: gathering region %s", monitor->name);
    }

    talp_info_t *talp_info = spd->talp_info;

    /* Reduce monitor among all MPI ranks into MPI rank 0 */
    pop_base_metrics_t base_metrics;
    talp_reduce_pop_metrics(&base_metrics, monitor, false);

    if (_mpi_rank == 0) {
        if (base_metrics.elapsed_time > 0) {

            /* Only the global region records the resources */
            if (monitor == talp_info->monitor) {
                talp_output_record_resources(base_metrics.num_cpus,
                        base_metrics.num_nodes, base_metrics.num_mpi_ranks);
            }

            /* Construct pop_metrics out of base metrics */
            dlb_pop_metrics_t pop_metrics;
            talp_base_metrics_to_pop_metrics(monitor->name, &base_metrics, &pop_metrics);

            /* Record */
            verbose(VB_TALP, "TALP summary: recording region %s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);

        } else {
            /* Record empty */
            verbose(VB_TALP, "TALP summary: recording empty region %s", monitor->name);
            dlb_pop_metrics_t pop_metrics = {0};
            snprintf(pop_metrics.name, DLB_MONITOR_NAME_MAX, "%s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);
        }
    }
}
#endif

#ifdef MPI_LIB
/* Communicate among all MPI processes so that everyone has the same monitoring regions */
static void talp_register_common_mpi_regions(const subprocess_descriptor_t *spd) {
    /* Note: there's a potential race condition if this function is called
     * (which happens on talp_mpi_finalize or talp_finalize) while another
     * thread creates a monitoring region. The solution would be to lock the
     * entire routine and call a specialized registering function that does not
     * lock, or use a recursive lock. The situation is strange enough to not
     * support it */

    talp_info_t *talp_info = spd->talp_info;

    /* Warn about open regions */
    for (GSList *node = talp_info->open_regions;
            node != NULL;
            node = node->next) {
        const dlb_monitor_t *monitor = node->data;
        warning("Region %s is still open during MPI_Finalize."
                " Collected data may be incomplete.",
                monitor->name);
    }

    /* Gather recvcounts for each process
        * (Each process may have different number of monitors) */
    int nregions = g_tree_nnodes(talp_info->regions);
    int chars_to_send = nregions * DLB_MONITOR_NAME_MAX;
    int *recvcounts = malloc(_mpi_size * sizeof(int));
    PMPI_Allgather(&chars_to_send, 1, MPI_INT,
            recvcounts, 1, MPI_INT, getWorldComm());

    /* Compute total characters to gather via MPI */
    int i;
    int total_chars = 0;
    for (i=0; i<_mpi_size; ++i) {
        total_chars += recvcounts[i];
    }

    if (total_chars > 0) {
        /* Prepare sendbuffer */
        char *sendbuffer = malloc(nregions * DLB_MONITOR_NAME_MAX * sizeof(char));
        char *sendptr = sendbuffer;
        for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                node != NULL;
                node = g_tree_node_next(node)) {
            const dlb_monitor_t *monitor = g_tree_node_value(node);
            strcpy(sendptr, monitor->name);
            sendptr += DLB_MONITOR_NAME_MAX;
        }

        /* Prepare recvbuffer */
        char *recvbuffer = malloc(total_chars * sizeof(char));

        /* Compute displacements */
        int *displs = malloc(_mpi_size * sizeof(int));
        int next_disp = 0;
        for (i=0; i<_mpi_size; ++i) {
            displs[i] = next_disp;
            next_disp += recvcounts[i];
        }

        /* Gather all regions */
        PMPI_Allgatherv(sendbuffer, nregions * DLB_MONITOR_NAME_MAX, MPI_CHAR,
                recvbuffer, recvcounts, displs, MPI_CHAR, getWorldComm());

        /* Register all regions. Existing ones will be skipped. */
        for (i=0; i<total_chars; i+=DLB_MONITOR_NAME_MAX) {
            monitoring_region_register(spd, &recvbuffer[i]);
        }

        free(sendbuffer);
        free(recvbuffer);
        free(displs);
    }

    free(recvcounts);
}
#endif


/*********************************************************************************/
/*    TALP MPI functions                                                         */
/*********************************************************************************/

/* Start global monitoring region (if not already started) */
void talp_mpi_init(const subprocess_descriptor_t *spd) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_mpi_init");

    /* Initialize MPI type */
#if MPI_LIB
# if MPI_VERSION >= 3
    mpi_int64_type = MPI_INT64_T;
# else
    PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(int64_t), &mpi_int64_type);
# endif
#endif

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_info->flags.have_mpi = true;

        /* Start global region (no-op if already started) */
        monitoring_region_start(spd, talp_info->monitor);

        /* Add MPI_Init statistic and set useful state */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

/* Stop global monitoring region and gather APP data if needed  */
void talp_mpi_finalize(const subprocess_descriptor_t *spd) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_mpi_finalize");
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Add MPI_Finalize */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);

        /* Stop global region */
        monitoring_region_stop(spd, talp_info->monitor);

        monitor_data_t *monitor_data = talp_info->monitor->_data;

        /* Update shared memory values */
        if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
            // TODO: is it needed? isn't it updated when stopped?
            shmem_talp__set_times(monitor_data->node_shared_id,
                    talp_info->monitor->mpi_time,
                    talp_info->monitor->useful_time);
        }

#ifdef MPI_LIB
        /* If performing any kind of TALP summary, check that the number of processes
         * registered in the shared memory matches with the number of MPI processes in the node.
         * This check is needed to avoid deadlocks on finalize. */
        if (spd->options.talp_summary) {
            verbose(VB_TALP, "Gathering TALP metrics");
            /* FIXME: use Named Barrier */
            /* if (shmem_barrier__get_num_participants(spd->options.barrier_id) == _mpis_per_node) { */

                /* Gather data among processes in the node if node summary is enabled */
                if (spd->options.talp_summary & SUMMARY_NODE) {
                    talp_record_node_summary(spd);
                }

                /* Gather data among MPIs if any of these summaries is enabled  */
                if (spd->options.talp_summary
                        & (SUMMARY_POP_METRICS | SUMMARY_PROCESS)) {
                    /* Ensure everyone has the same monitoring regions */
                    talp_register_common_mpi_regions(spd);

                    /* Finally, reduce data */
                    for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                            node != NULL;
                            node = g_tree_node_next(node)) {
                        const dlb_monitor_t *monitor = g_tree_node_value(node);
                        if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
                            talp_record_pop_summary(spd, monitor);
                        }
                        if (spd->options.talp_summary & SUMMARY_PROCESS) {
                            talp_record_process_summary(spd, monitor);
                        }
                    }
                }

                /* Synchronize all processes in node before continuing with DLB finalization  */
                node_barrier(spd, NULL);
            /* } else { */
            /*     warning("The number of MPI processes and processes registered in DLB differ." */
            /*             " TALP will not print any summary."); */
            /* } */
        }
#endif
    }
}

/* Decide whether to update the current sample (most likely and cheaper)
 * or to aggregate all samples.
 * We will only aggregate all samples if the external profiler is enabled
 * and this MPI call is a blocking collective call. */
static inline void update_sample_on_sync_call(const subprocess_descriptor_t *spd,
        const talp_info_t *talp_info, talp_sample_t *sample, bool is_blocking_collective) {

    if (!talp_info->flags.external_profiler || !is_blocking_collective) {
        /* Likely scenario, just update the sample */
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);
    } else {
        /* If talp_info->flags.external_profiler && is_blocking_collective:
         * aggregate samples and update all monitoring regions */
        talp_aggregate_samples(spd);
    }
}

void talp_into_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        update_sample_on_sync_call(spd, talp_info, sample, is_blocking_collective);

        /* Into Sync call -> not_useful_mpi */
        set_sample_state(sample, not_useful_mpi, talp_info->flags.papi);
    }
}

void talp_out_of_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);
        update_sample_on_sync_call(spd, talp_info, sample, is_blocking_collective);

        /* Out of Sync call -> useful */
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}


/*********************************************************************************/
/*    TALP OpenMP functions                                                      */
/*********************************************************************************/

/* samples involved in parallel level 1 */
static talp_sample_t** parallel_samples_l1 = NULL;
static unsigned int parallel_samples_l1_capacity = 0;

void talp_openmp_init(pid_t pid, const options_t* options) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_openmp_init");

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        monitor_data_t *monitor_data = talp_info->monitor->_data;
        talp_info->flags.have_openmp = true;

        /* Fix up number of CPUs for the global region */
        float cpus = CPU_COUNT(&spd->process_mask);
        talp_info->monitor->avg_cpus = cpus;
        shmem_talp__set_avg_cpus(monitor_data->node_shared_id, cpus);

        /* Start global region (no-op if already started) */
        monitoring_region_start(spd, talp_info->monitor);

        /* Set useful state */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_finalize(void) {
    if (parallel_samples_l1 != NULL) {
        free(parallel_samples_l1);
        parallel_samples_l1 = NULL;
        parallel_samples_l1_capacity = 0;
    }
}

void talp_openmp_thread_begin(ompt_thread_t thread_type) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Initial thread is already in useful state, set omp_out for others */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        if (sample->state == disabled) {
            /* Not initial thread: */
            if (talp_info->flags.papi) {
                init_papi_counters();
            }
            set_sample_state(sample, not_useful_omp_out, talp_info->flags.papi);

            /* The initial time of the sample is set to match the start time of
             * the innermost open region, but other nested open regions need to
             * be fixed */
            monitoring_regions_update_nested(spd, sample);
        }
    }
}

void talp_openmp_thread_end(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, disabled, talp_info->flags.papi);
    }
}

void talp_openmp_parallel_begin(omptool_parallel_data_t *parallel_data) {
    fatal_cond(parallel_data->requested_parallelism < 1,
            "Requested parallel region of invalid size in %s. Please report bug at %s.",
            __func__, PACKAGE_BUGREPORT);

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (parallel_data->level == 1) {
            /* Resize samples of parallel 1 if needed */
            unsigned int requested_parallelism = parallel_data->requested_parallelism;
            if (requested_parallelism > parallel_samples_l1_capacity) {
                void *ptr = realloc(parallel_samples_l1,
                        sizeof(talp_sample_t*)*requested_parallelism);
                fatal_cond(!ptr, "realloc failed in %s", __func__);
                parallel_samples_l1 = ptr;
                parallel_samples_l1_capacity = requested_parallelism;
            }

            /* Assign local data */
            parallel_data->talp_parallel_data = parallel_samples_l1;

        } else if (parallel_data->level > 1) {
            /* Allocate parallel samples array */
            unsigned int requested_parallelism = parallel_data->requested_parallelism;
            void *ptr = malloc(sizeof(talp_sample_t*)*requested_parallelism);
            fatal_cond(!ptr, "malloc failed in %s", __func__);

            /* Assign local data */
            parallel_data->talp_parallel_data = ptr;
        }

        /* Update stats */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_omp_parallels, 1);
    }
}

void talp_openmp_parallel_end(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        if (parallel_data->level == 1) {
            /* Flush and aggregate all samples of the parallel region */
            talp_aggregate_samples_parallel(spd,
                    parallel_data->talp_parallel_data,
                    parallel_data->actual_parallelism);

        } else if (parallel_data->level > 1) {
            /* Flush and aggregate all samples of this parallel except this
             * thread's sample. The primary thread of a nested parallel region
             * will keep its samples until it finishes as non-primary
             * team-worker or reaches the level 1 parallel region */
            talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
            talp_aggregate_samples_parallel(spd,
                    &parallel_samples[1],
                    parallel_data->actual_parallelism-1);

            /* free local data */
            free(parallel_data->talp_parallel_data);
            parallel_data->talp_parallel_data = NULL;
        }

        /* Update current threads's state */
        set_sample_state(sample, useful, talp_info->flags.papi);

        /* Update the state of the rest of team-worker threads
         * (note that set_sample_state cannot be used here because we are
         * impersonating a worker thread) */
        talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
        for (unsigned int i = 1; i < parallel_data->actual_parallelism; ++i) {
            talp_sample_t *worker_sample = parallel_samples[i];
            if (worker_sample->state == not_useful_omp_in) {
                worker_sample->state = not_useful_omp_out;
            }
        }
    }
}

void talp_openmp_into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Assign thread sample as team-worker of this parallel */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
        /* Probably optimized, but try to avoid invalidating
         * the cache line on reused parallel data */
        if (parallel_samples[index] != sample) {
            parallel_samples[index] = sample;
        }

        /* Update thread sample with the last microsample */
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_outof_parallel_function(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, not_useful_omp_out, talp_info->flags.papi);
    }
}

void talp_openmp_into_parallel_implicit_barrier(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_into_parallel_sync(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_outof_parallel_sync(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_task_create(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Just update stats */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_omp_tasks, 1);
    }
}

void talp_openmp_task_complete(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state (FIXME: tasks outside of parallels? */
        set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_task_switch(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, NO_TIMESTAMP);

        /* Update state */
        set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

/*********************************************************************************/
/*    TALP collect functions for 3rd party programs:                             */
/*      - It's also safe to call it from a 1st party program                     */
/*      - Requires --talp-external-profiler set up in application                */
/*      - Des not need to synchronize with application                           */
/*********************************************************************************/

/* Function that may be called from a third-party process to compute
 * node_metrics for a given region */
int talp_query_pop_node_metrics(const char *name, dlb_node_metrics_t *node_metrics) {

    if (name == NULL) {
        name = global_region_name;
    }

    int error = DLB_SUCCESS;
    int64_t total_mpi_time = 0;
    int64_t total_useful_time = 0;
    int64_t max_mpi_time = 0;
    int64_t max_useful_time = 0;

    /* Obtain a list of regions in the node associated with given region */
    int max_procs = mu_get_system_size();
    talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
    int nelems;
    shmem_talp__get_regionlist(region_list, &nelems, max_procs, name);

    /* Count how many processes have started the region */
    int processes_per_node = 0;

    /* Iterate the PID list and gather times of every process */
    int i;
    for (i = 0; i <nelems; ++i) {
        int64_t mpi_time = region_list[i].mpi_time;
        int64_t useful_time = region_list[i].useful_time;

        /* Accumulate total and max values */
        if (mpi_time > 0 || useful_time > 0) {
            ++processes_per_node;
            total_mpi_time += mpi_time;
            total_useful_time += useful_time;
            max_mpi_time = max_int64(mpi_time, max_mpi_time);
            max_useful_time = max_int64(useful_time, max_useful_time);
        }
    }
    free(region_list);

#if MPI_LIB
    int node_id = _node_id;
#else
    int node_id = 0;
#endif

    if (processes_per_node > 0) {
        /* Compute POP metrics with some inferred values */
        perf_metrics_mpi_t metrics;
        perf_metrics__infer_mpi_model(
                &metrics,
                processes_per_node,
                total_useful_time,
                total_mpi_time,
                max_useful_time);

        /* Initialize structure */
        *node_metrics = (const dlb_node_metrics_t) {
            .node_id = node_id,
            .processes_per_node = processes_per_node,
            .total_useful_time = total_useful_time,
            .total_mpi_time = total_mpi_time,
            .max_useful_time = max_useful_time,
            .max_mpi_time = max_mpi_time,
            .parallel_efficiency = metrics.parallel_efficiency,
            .communication_efficiency = metrics.communication_efficiency,
            .load_balance = metrics.load_balance,
        };
        snprintf(node_metrics->name, DLB_MONITOR_NAME_MAX, "%s", name);
    } else {
        error = DLB_ERR_NOENT;
    }

    return error;
}



/*********************************************************************************/
/*    TALP collect functions for 1st party programs                              */
/*      - Requires synchronization (MPI or node barrier) among all processes     */
/*********************************************************************************/

/* Perform MPI collective calls and compute the current POP metrics for the
 * specified monitor. If monitor is NULL, the global monitoring region is
 * assumed.
 * Pre-conditions:
 *  - the given monitor must have been registered in all MPI ranks
 *  - pop_metrics is an allocated structure
 */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
#ifdef MPI_LIB
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == NULL) {
        monitor = talp_info->monitor;
    }

    /* Stop monitor so that metrics are updated */
    bool resume_region = monitoring_region_stop(spd, monitor) == DLB_SUCCESS;

    /* Reduce monitor among all MPI ranks and everbody collects (all-to-all) */
    pop_base_metrics_t base_metrics;
    talp_reduce_pop_metrics(&base_metrics, monitor, true);

    /* Construct output pop_metrics out of base metrics */
    talp_base_metrics_to_pop_metrics(monitor->name, &base_metrics, pop_metrics);

    /* Resume monitor */
    if (resume_region) {
        monitoring_region_start(spd, monitor);
    }

    return DLB_SUCCESS;
#else
    return DLB_ERR_NOCOMP;
#endif
}

/* Node-collective function to compute node_metrics for a given region */
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {

    talp_info_t *talp_info = spd->talp_info;
    monitor = monitor ? monitor : talp_info->monitor;
    monitor_data_t *monitor_data = monitor->_data;

    /* Stop monitor so that metrics are updated */
    bool resume_region = monitoring_region_stop(spd, monitor) == DLB_SUCCESS;

    /* This functionality needs a shared memory, create a temporary one if needed */
    if (!talp_info->flags.have_shmem) {
        shmem_talp__init(spd->options.shm_key, 1);
        shmem_talp__register(spd->id, monitor->avg_cpus, monitor->name,
                &monitor_data->node_shared_id);
    }

    /* Update the shared memory with this process' metrics */
    shmem_talp__set_times(monitor_data->node_shared_id,
            monitor->mpi_time,
            monitor->useful_time);

    /* Perform a node barrier to ensure everyone has updated their metrics */
    node_barrier(spd, NULL);

    /* Compute node metrics for that region name */
    talp_query_pop_node_metrics(monitor->name, node_metrics);

    /* Remove shared memory if it was a temporary one */
    if (!talp_info->flags.have_shmem) {
        shmem_talp__finalize(spd->id);
    }

    /* Resume monitor */
    if (resume_region) {
        monitoring_region_start(spd, monitor);
    }

    return DLB_SUCCESS;
}
