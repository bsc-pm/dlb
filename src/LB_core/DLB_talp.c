/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/gtree.h"
#include "support/gslist.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/talp_output.h"
#include "LB_comm/shmem_talp.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>

#ifdef PAPI_LIB
#include <papi.h>
#endif

/* The macrosample contains the accumulated metrics of samples of all threads.
 * It is only constructed when samples are gathered, and then it is used to
 * update all started monitoring regions. */
typedef struct talp_macrosample_t {
    int64_t mpi_time;
    int64_t useful_time;
    int64_t elapsed_useful_time;
    int64_t num_mpi_calls;
#ifdef PAPI_LIB
    int64_t cycles;
    int64_t instructions;
#endif
} talp_macrosample_t;

/* Private data per monitor */
typedef struct monitor_data_t {
    int     id;
    int     node_shared_id;     /* id for allocating region in the shmem */
    bool    started;
} monitor_data_t;


static void talp_dealloc_samples(const subprocess_descriptor_t *spd);
static void init_regions(void);
static void monitoring_region_initialize(dlb_monitor_t *monitor, int id,
        const char *name, pid_t pid, bool have_shmem);
static void monitoring_regions_finalize_all(const subprocess_descriptor_t *spd);
static void monitoring_regions_update_all(const subprocess_descriptor_t *spd,
        const talp_macrosample_t *macrosample);
static void monitoring_regions_report_all(const subprocess_descriptor_t *spd);

#if MPI_LIB
static void monitoring_regions_gather_data(const subprocess_descriptor_t *spd);
static void talp_node_summary_gather_data(const subprocess_descriptor_t *spd);

static MPI_Datatype mpi_int64_type;
#endif

extern __thread bool thread_is_observer;

#if PAPI_LIB
static int EventSet = PAPI_NULL;
#endif

const char *mpi_region_name = "MPI Execution";

/* Reserved regions ids */
enum { MPI_MONITORING_REGION_ID = 1 };

/* Dynamic region ids */
static int get_new_monitor_id(void) {
    static atomic_int id = MPI_MONITORING_REGION_ID;
    return DLB_ATOMIC_ADD_FETCH_RLX(&id, 1);
}

/* Unique anonymous regions */
static int get_anonymous_id(void) {
    static atomic_int id = 0;
    return DLB_ATOMIC_ADD_FETCH_RLX(&id, 1);
}


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

#ifdef PAPI_LIB
static inline int init_papi(void) {
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

    error = PAPI_register_thread();
    if (error != PAPI_OK) {
        warning("PAPI Error during thread registration. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    /* Eventset creation */
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

    return 0;
}
#endif

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .external_profiler = spd->options.talp_external_profiler,
        .papi = spd->options.talp_papi,
        .have_shmem = spd->options.talp_external_profiler
            || spd->options.talp_summary & SUMMARY_NODE,
        .samples_mutex = PTHREAD_MUTEX_INITIALIZER,
    };
    spd->talp_info = talp_info;

    /* Initialize shared memory */
    if (talp_info->have_shmem) {
        shmem_talp__init(spd->options.shm_key, spd->options.talp_regions_per_proc);
    }

    /* Initialize MPI monitor */
    init_regions();
    monitoring_region_initialize(&talp_info->mpi_monitor,
            MPI_MONITORING_REGION_ID, mpi_region_name, spd->id, talp_info->have_shmem);

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&spd->process_mask));

    /* Initialize and start running PAPI */
    if (talp_info->papi) {
#ifdef PAPI_LIB
        if (init_papi() != 0) {
            warning("PAPI initialization has failed, disabling option.");
            talp_info->papi = false;
        }
#else
        warning("DLB has not been configured with PAPI support, disabling option.");
        talp_info->papi = false;
#endif
    }
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_finalize");
    verbose(VB_TALP, "Finalizing TALP module");

    if (spd->options.talp_summary & SUMMARY_PROCESS
        || spd->options.talp_summary & SUMMARY_POP_METRICS
        || spd->options.talp_summary & SUMMARY_POP_RAW) {
        monitoring_regions_report_all(spd);
    }

    /* Print/write all collected summaries */
    talp_output_finalize(spd->options.talp_output_file);

    /* Deallocate samples structure */
    talp_dealloc_samples(spd);

    /* Finalize shared memory */
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info->have_shmem) {
        shmem_talp__finalize(spd->id);
    }

#ifdef PAPI_LIB
    if (talp_info->papi) {
        PAPI_shutdown();
    }
#endif

    /* Deallocate monitoring regions and talp_info */
    monitoring_regions_finalize_all(spd);
    free(spd->talp_info);
    spd->talp_info = NULL;
}


/*********************************************************************************/
/*    Sample functions                                                           */
/*********************************************************************************/

static __thread talp_sample_t* _tls_sample = NULL;

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

    *_tls_sample = (const talp_sample_t) {
        .in_useful = true,
    };

    return _tls_sample;
}

static void talp_dealloc_samples(const subprocess_descriptor_t *spd) {
    /* This only nullifies the current thread pointer, but this function is
     * only really important when TALP is finalized and then initialized again
     */
    _tls_sample = NULL;

    /* */
    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        free(talp_info->samples);
        talp_info->samples = NULL;
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);
}

/* Compute new microsample (time since last update) and update sample values */
static void talp_update_sample(const subprocess_descriptor_t *spd, talp_sample_t *sample,
        bool papi) {
    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_timestamp */
    int64_t now = get_time_in_ns();
    int64_t microsample_duration = now - sample->last_updated_timestamp;
    sample->last_updated_timestamp = now;

    /* Compute MPI time */
    int64_t mpi_time = sample->in_useful ? 0 : microsample_duration;

    /* Compute useful time */
    int64_t useful_time = sample->in_useful ? microsample_duration : 0;

    /* Update sample */
    DLB_ATOMIC_ADD_RLX(&sample->mpi_time, mpi_time);
    DLB_ATOMIC_ADD_RLX(&sample->useful_time, useful_time);

#ifdef PAPI_LIB
    if (papi) {
        /* Read counters only if the sample is in_useful */
        if (sample->in_useful) {
            long long papi_values[2];
            int error = PAPI_read(EventSet, papi_values);
            if (error != PAPI_OK) {
                verbose(VB_TALP, "stop return code: %d, %s", error, PAPI_strerror(error));
            }

            /* Atomically add papi_values to structure */
            DLB_ATOMIC_ADD_RLX(&sample->cycles, papi_values[0]);
            DLB_ATOMIC_ADD_RLX(&sample->instructions, papi_values[1]);
        }

        /* Reset counters and restart counting */
        int error = PAPI_reset(EventSet);
        if (error != PAPI_OK) verbose(VB_TALP, "Error resetting counters");
    }
#endif
}

/* Accumulate values from samples of all threads and update regions */
static void talp_gather_samples(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    talp_sample_t *thread_sample = talp_get_thread_sample(spd);

    /* Update thread sample with the last microsample */
    talp_update_sample(spd, thread_sample, talp_info->papi);

    /* Accumulate samples from all threads */
    talp_macrosample_t macrosample = (const talp_macrosample_t) {};
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        int i;
        for (i=0; i<talp_info->ncpus; ++i) {
            talp_sample_t *sample = talp_info->samples[i];

            /* Atomically extract and reset sample values */
            int64_t mpi_time = DLB_ATOMIC_EXCH_RLX(&sample->mpi_time, 0);
            int64_t useful_time = DLB_ATOMIC_EXCH_RLX(&sample->useful_time, 0);
            int64_t num_mpi_calls = DLB_ATOMIC_EXCH_RLX(&sample->num_mpi_calls, 0);
#ifdef PAPI_LIB
            int64_t cycles = DLB_ATOMIC_EXCH_RLX(&sample->cycles, 0);
            int64_t instructions = DLB_ATOMIC_EXCH_RLX(&sample->instructions, 0);
#endif

            /* Accumulate */
            macrosample.mpi_time += mpi_time;
            macrosample.useful_time += useful_time;
            macrosample.elapsed_useful_time = max_int64(
                    macrosample.elapsed_useful_time, useful_time);
            macrosample.num_mpi_calls += num_mpi_calls;
#ifdef PAPI_LIB
            macrosample.cycles += cycles;
            macrosample.instructions += instructions;
#endif
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    /* Update all started regions */
    monitoring_regions_update_all(spd, &macrosample);
}


/*********************************************************************************/
/*    TALP MPI functions                                                         */
/*********************************************************************************/

/* Start MPI monitoring region */
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
        /* Start MPI region */
        monitoring_region_start(spd, &talp_info->mpi_monitor);

        /* Add MPI_Init */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->num_mpi_calls, 1);
    }
}

/* Stop MPI monitoring region and gather APP data if needed  */
void talp_mpi_finalize(const subprocess_descriptor_t *spd) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_mpi_finalize");
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Add MPI_Finalize */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->num_mpi_calls, 1);

        /* Stop MPI region */
        monitoring_region_stop(spd, &talp_info->mpi_monitor);

        /* Update shared memory values */
        if (talp_info->have_shmem) {
            // TODO: is it needed? isn't it updated when stopped?
            monitor_data_t *monitor_data = talp_info->mpi_monitor._data;
            shmem_talp__set_times(monitor_data->node_shared_id,
                    talp_info->mpi_monitor.accumulated_MPI_time,
                    talp_info->mpi_monitor.accumulated_computation_time);
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
                    talp_node_summary_gather_data(spd);
                }

                /* Gather data among MPIs if any of these summaries is enabled  */
                if (spd->options.talp_summary
                        & (SUMMARY_POP_METRICS | SUMMARY_POP_RAW | SUMMARY_PROCESS)) {
                    monitoring_regions_gather_data(spd);
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

void talp_into_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_sample_t *sample = talp_get_thread_sample(spd);
        if (!is_blocking_collective || !talp_info->external_profiler) {
            /* Either this MPI call is not a blocking collective or the
             * external_profiler is disabled: just update the sample */
            talp_update_sample(spd, sample, talp_info->papi);
        } else {
            /* Otherwise, gather samples and update all monitoring regions */
            talp_gather_samples(spd);
        }
        sample->in_useful = false;
    }
}

void talp_out_of_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->num_mpi_calls, 1);
        if (!is_blocking_collective || !talp_info->external_profiler) {
            /* Either this MPI call is not a blocking collective or the
             * external_profiler is disabled: just update the sample */
            talp_update_sample(spd, sample, talp_info->papi);
        } else {
            /* Otherwise, gather samples and update all monitoring regions */
            talp_gather_samples(spd);
        }
        sample->in_useful = true;
    }
}


/*********************************************************************************/
/*    Other functions                                                            */
/*********************************************************************************/

#if MPI_LIB
static void talp_node_summary_gather_data(const subprocess_descriptor_t *spd) {
    /* Node summary */
    typedef struct monitor_node_summary_t {
        int nelems;
        int64_t total_mpi_time;
        int64_t total_useful_time;
        int64_t max_mpi_time;
        int64_t max_useful_time;
        process_in_node_record_t process_info[];
    } monitor_node_summary_t;

    monitor_node_summary_t *node_summary = NULL;

    /* Perform a barrier so that all processes in the node have arrived at the
     * MPI_Finalize */
    node_barrier(spd, NULL);

    if (_process_id == 0) {
        /* Obtain a list of regions associated with "MPI Execution", sorted by PID */
        int max_procs = mu_get_system_size();
        talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
        int nelems;
        shmem_talp__get_regionlist(region_list, &nelems, max_procs, mpi_region_name);

        /* Allocate node summary structure */
        node_summary = malloc(
                sizeof(monitor_node_summary_t) + sizeof(process_in_node_record_t) * nelems);
        node_summary->nelems = nelems;

        int64_t total_mpi_time = 0;
        int64_t total_useful_time = 0;
        int64_t max_mpi_time = 0;
        int64_t max_useful_time = 0;

        /* Iterate the PID list and gather times of every process */
        int i;
        for (i = 0; i <nelems; ++i) {
            int64_t mpi_time = region_list[i].mpi_time;
            int64_t useful_time = region_list[i].useful_time;

            /* Save times in local structure */
            node_summary->process_info[i].pid = region_list[i].pid;
            node_summary->process_info[i].mpi_time = mpi_time;
            node_summary->process_info[i].useful_time = useful_time;

            /* Accumulate total and max values */
            total_mpi_time += mpi_time;
            total_useful_time += useful_time;
            max_mpi_time = max_int64(mpi_time, max_mpi_time);
            max_useful_time = max_int64(useful_time, max_useful_time);
        }

        /* Save accumulated values */
        node_summary->total_mpi_time = total_mpi_time;
        node_summary->total_useful_time = total_useful_time;
        node_summary->max_mpi_time = max_mpi_time;
        node_summary->max_useful_time = max_useful_time;

        free(region_list);
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
            int count = 2;
            int blocklengths[] = {1, 2};
            MPI_Aint displacements[] = {
                offsetof(process_in_node_record_t, pid),
                offsetof(process_in_node_record_t, mpi_time)};
            MPI_Datatype types[] = {mpi_pid_type, mpi_int64_type};
            PMPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_process_info_type);
            PMPI_Type_commit(&mpi_process_info_type);
        }

        /* MPI struct type: monitor_node_summary_t */
        MPI_Datatype mpi_node_summary_type;
        {
            int count = 3;
            int blocklengths[] = {1, 4, node_summary->nelems};
            MPI_Aint displacements[] = {
                offsetof(monitor_node_summary_t, nelems),
                offsetof(monitor_node_summary_t, total_mpi_time),
                offsetof(monitor_node_summary_t, process_info)};
            MPI_Datatype types[] = {MPI_INT, mpi_int64_type, mpi_process_info_type};
            PMPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_node_summary_type);
            PMPI_Type_commit(&mpi_node_summary_type);
        }

        /* Gather data */
        void *recvbuf = NULL;
        size_t node_summary_size = sizeof(monitor_node_summary_t)
                + sizeof(process_in_node_record_t) * node_summary->nelems;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_num_nodes * node_summary_size);
        }
        PMPI_Gather(node_summary, 1, mpi_node_summary_type,
                recvbuf, 1, mpi_node_summary_type,
                0, getInterNodeComm());

        /* Free send buffer */
        free(node_summary);

        /* Add records */
        if (_mpi_rank == 0) {
            int node_id;
            for (node_id=0; node_id<_num_nodes; ++node_id) {
                verbose(VB_TALP, "Node summary: recording node %d", node_id);
                node_summary = recvbuf + node_summary_size*node_id;
                talp_output_record_node(
                        node_id,
                        node_summary->nelems,
                        node_summary->total_useful_time / node_summary->nelems,
                        node_summary->total_mpi_time / node_summary->nelems,
                        node_summary->max_useful_time,
                        node_summary->max_mpi_time,
                        node_summary->process_info);
            }
            free(recvbuf);
        }
    }
}
#endif


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

static GTree *regions = NULL;
static GSList *open_regions = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Compare region names */
static gint key_compare_func(gconstpointer a, gconstpointer b) {
    return strncmp(a, b, DLB_MONITOR_NAME_MAX-1);
}

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

static void init_regions(void) {
    fatal_cond(regions != NULL || open_regions != NULL,
            "Error initializing regions. Please report bug at "PACKAGE_BUGREPORT);

    /* binary tree with all regions */
    regions = g_tree_new_full(
            (GCompareDataFunc)key_compare_func,
            NULL, NULL, dealloc_region);
}

const struct dlb_monitor_t* monitoring_region_get_MPI_region(
        const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    return talp_info ? &talp_info->mpi_monitor : NULL;
}

const char* monitoring_region_get_MPI_region_name(void) {
    return mpi_region_name;
}

dlb_monitor_t* monitoring_region_register(const subprocess_descriptor_t *spd,
        const char* name) {

    dlb_monitor_t *monitor;
    bool anonymous_region = (name == NULL || *name == '\0');

    /* Found monitor if already registered */
    if (!anonymous_region) {
        pthread_mutex_lock(&mutex);
        {
            monitor = g_tree_lookup(regions, name);
        }
        pthread_mutex_unlock(&mutex);

        if (monitor != NULL) {
            return monitor;
        }
    }

    /* Otherwise, create new monitoring region */
    monitor = malloc(sizeof(dlb_monitor_t));
    fatal_cond(!monitor, "Could not register a new monitoring region."
            " Please report at "PACKAGE_BUGREPORT);

    /* Initialize values */
    talp_info_t *talp_info = spd->talp_info;
    if (anonymous_region) {
        char monitor_name[DLB_MONITOR_NAME_MAX];
        snprintf(monitor_name, DLB_MONITOR_NAME_MAX, "Anonymous Region %d", get_anonymous_id());
        monitoring_region_initialize(monitor, get_new_monitor_id(), monitor_name, spd->id,
                talp_info->have_shmem);
    } else {
        monitoring_region_initialize(monitor, get_new_monitor_id(), name, spd->id,
                talp_info->have_shmem);
    }

    /* Finally, insert */
    pthread_mutex_lock(&mutex);
    {
        g_tree_insert(regions, (gpointer)monitor->name, monitor);
    }
    pthread_mutex_unlock(&mutex);

    return monitor;
}

static void monitoring_region_initialize(dlb_monitor_t *monitor,
        int id, const char *name, pid_t pid, bool have_shmem) {
    /* Initialize private monitor data */
    monitor_data_t *monitor_data = malloc(sizeof(monitor_data_t));
    *monitor_data = (const monitor_data_t) {
        .id = id,
        .node_shared_id = -1,
    };

    /* Allocate monitor name */
    char *allocated_name = malloc(DLB_MONITOR_NAME_MAX*sizeof(char));
    snprintf(allocated_name, DLB_MONITOR_NAME_MAX, "%s", name);

    /* Initialize monitor */
    *monitor = (const dlb_monitor_t) {
            .name = allocated_name,
            ._data = monitor_data,
    };

    /* Register name in the instrumentation tool */
    instrument_register_event(MONITOR_REGION, monitor_data->id, name);

    /* Register region in shmem */
    if (have_shmem) {
        int err = shmem_talp__register(pid, monitor->name, &monitor_data->node_shared_id);
        if (err == DLB_ERR_NOMEM) {
            warning("Region %s has been correctly registered but cannot be shared among other"
                    " processes due to lack of space in the TALP shared memory. Features like"
                    " node report or gathering data from external processes may not work for"
                    " this region. If needed, increase the TALP shared memory capacity using"
                    " the flag --talp-regions-per-proc. Run dlb -hh for more info.",
                    monitor->name);
        } else if (err < DLB_SUCCESS) {
            fatal("Unknown error registering region %s, please report bug at %s",
                    monitor->name, PACKAGE_BUGREPORT);
        }
    }
}

int monitoring_region_reset(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == DLB_MPI_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = &talp_info->mpi_monitor;
    }

    /* Reset everything except these fields: */
    *monitor = (const dlb_monitor_t) {
        .name = monitor->name,
        .num_resets = monitor->num_resets + 1,
        ._data = monitor->_data,
    };

    monitor_data_t *monitor_data = monitor->_data;
    monitor_data->started = false;

    return DLB_SUCCESS;
}

int monitoring_region_start(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == DLB_MPI_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = &talp_info->mpi_monitor;
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->started) {
        /* Gather samples from all threads and update regions */
        talp_gather_samples(spd);

        verbose(VB_TALP, "Starting region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_BEGIN);

        monitor->start_time = get_time_in_ns();
        monitor->stop_time = 0;

        pthread_mutex_lock(&mutex);
        {
            monitor_data->started = true;
            open_regions = g_slist_prepend(open_regions, monitor);
        }
        pthread_mutex_unlock(&mutex);

        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_stop(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == DLB_MPI_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = &talp_info->mpi_monitor;
    } else if (monitor == DLB_LAST_OPEN_REGION) {
        if (open_regions != NULL) {
            monitor = open_regions->data;
        } else {
            return DLB_ERR_NOENT;
        }
    }

    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (monitor_data->started) {
        /* Gather samples from all threads and update regions */
        talp_gather_samples(spd);

        /* Stop timer */
        monitor->stop_time = get_time_in_ns();
        monitor->elapsed_time += monitor->stop_time - monitor->start_time;
        ++(monitor->num_measurements);

        pthread_mutex_lock(&mutex);
        {
            monitor_data->started = false;
            open_regions = g_slist_remove(open_regions, monitor);
        }
        pthread_mutex_unlock(&mutex);

        verbose(VB_TALP, "Stopping region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_END);
        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

bool monitoring_region_is_started(const dlb_monitor_t *monitor) {
    return ((monitor_data_t*)monitor->_data)->started;
}

int monitoring_region_report(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor) {
    if (monitor == DLB_MPI_REGION) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = &talp_info->mpi_monitor;
    }

    info("########### Monitoring Region Summary ###########");
    info("### Name:                       %s", monitor->name);
    info("### Elapsed time :              %.9g seconds",
            nsecs_to_secs(monitor->elapsed_time));
    info("### Elapsed computation time :  %.9g seconds",
            nsecs_to_secs(monitor->elapsed_computation_time));
    info("### MPI time :                  %.9g seconds",
            nsecs_to_secs(monitor->accumulated_MPI_time));
    info("### Computation time :          %.9g seconds",
            nsecs_to_secs(monitor->accumulated_computation_time));
    info("###      CpuSet:  %s", mu_to_str(&spd->process_mask));
#ifdef PAPI_LIB
    float ipc = monitor->accumulated_cycles == 0 ? 0.0f
        : (float)monitor->accumulated_instructions / monitor->accumulated_cycles;
    info("### IPC:                        %.2f ", ipc);
#endif
    return DLB_SUCCESS;
}

int monitoring_regions_force_update(const subprocess_descriptor_t *spd) {
    talp_gather_samples(spd);
    return DLB_SUCCESS;
}

static void monitoring_regions_finalize_all(const subprocess_descriptor_t *spd) {
    /* Finalize MPI monitor */
    talp_info_t *talp_info = spd->talp_info;
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;
    free(mpi_monitor->_data);
    free((char*)mpi_monitor->name);
    *mpi_monitor = (const dlb_monitor_t) {};

    /* De-allocate regions */
    pthread_mutex_lock(&mutex);
    {
        g_tree_destroy(regions);
        regions = NULL;
        g_slist_free(open_regions);
        open_regions = NULL;
    }
    pthread_mutex_unlock(&mutex);
}

static void monitoring_regions_update_all(const subprocess_descriptor_t *spd,
        const talp_macrosample_t *macrosample) {
    talp_info_t *talp_info = spd->talp_info;

    /* Update all open regions */
    pthread_mutex_lock(&mutex);
    {
        for (GSList *node = open_regions;
                node != NULL;
                node = node->next) {
            dlb_monitor_t *monitor = node->data;
            monitor_data_t *monitor_data = monitor->_data;

            monitor->elapsed_computation_time += macrosample->elapsed_useful_time;
            monitor->accumulated_MPI_time += macrosample->mpi_time;
            monitor->accumulated_computation_time += macrosample->useful_time;
            monitor->num_mpi_calls += macrosample->num_mpi_calls;
#ifdef PAPI_LIB
            monitor->accumulated_cycles += macrosample->cycles;
            monitor->accumulated_instructions += macrosample->instructions;
#endif

            /* Update shared memory only if requested */
            if (talp_info->external_profiler) {
                shmem_talp__set_times(monitor_data->node_shared_id,
                        monitor->accumulated_MPI_time,
                        monitor->accumulated_computation_time);
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void monitoring_regions_report_all(const subprocess_descriptor_t *spd) {
#ifdef MPI_LIB
    /* If MPI is enabled, all TALP reports are already managed by talp_output */
#else
    /* Report MPI monitor and custom regions */
    if (spd->options.talp_summary & SUMMARY_PROCESS) {
        if (spd->options.talp_output_file != NULL) {
            warning("Process' regions to file not supported in non MPI library");
        }
        talp_info_t *talp_info = spd->talp_info;
        monitoring_region_report(spd, &talp_info->mpi_monitor);

        for (GTreeNode *node = g_tree_node_first(regions);
                node != NULL;
                node = g_tree_node_next(node)) {
            const dlb_monitor_t *monitor = g_tree_node_value(node);
            monitoring_region_report(spd, monitor);
        }
    }

    if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
        warning("Option --talp-summary=pop-metrics is set but DLB is not intercepting MPI calls.");
        warning("Use a DLB library with MPI support or set --talp-summary=none to hide this warning.");
    }

    if (spd->options.talp_summary & SUMMARY_POP_RAW) {
        warning("Option --talp-summary=pop-raw is set but DLB is not intercepting MPI calls.");
        warning("Use a DLB library with MPI support.");
    }
#endif
}

#ifdef MPI_LIB
/* Gather data of a monitor among all ranks and record it in rank 0 */
static void gather_monitor_data(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor) {

    /* Note: we may be sending monitors info twice for PROCESS and POP reports
     * but reducing by MPI communicators makes the implementation much easier
     * and we don't expect the PROCESS summary to be very common */

    /* SUMMARY PROCESS: Collect monitor from all processes and record them all */
    if (spd->options.talp_summary & SUMMARY_PROCESS) {
        if (_mpi_rank == 0) {
            verbose(VB_TALP, "Process summary: gathering region %s", monitor->name);
        }

        /* Each process creates a serialized monitor to send to Rank 0 */
        enum { CPUSET_MAX = 64 };
        typedef struct serialized_monitor_t {
            char    hostname[HOST_NAME_MAX];
            char    cpuset[CPUSET_MAX];
            char    cpuset_quoted[CPUSET_MAX];
            int     pid;
            int     num_measurements;
            float   ipc;
            int64_t elapsed_time;
            int64_t elapsed_computation_time;
            int64_t accumulated_MPI_time;
            int64_t accumulated_computation_time;
        } serialized_monitor_t;

        float ipc = monitor->accumulated_cycles == 0 ? 0.0f
            : (float)monitor->accumulated_instructions / monitor->accumulated_cycles;

        serialized_monitor_t serialized_monitor = (const serialized_monitor_t) {
            .pid = spd->id,
            .num_measurements = monitor->num_measurements,
            .ipc = ipc,
            .elapsed_time = monitor->elapsed_time,
            .elapsed_computation_time = monitor->elapsed_computation_time,
            .accumulated_MPI_time = monitor->accumulated_MPI_time,
            .accumulated_computation_time = monitor->accumulated_computation_time,
        };

       gethostname(serialized_monitor.hostname, HOST_NAME_MAX);
       snprintf(serialized_monitor.cpuset, CPUSET_MAX, "%s", mu_to_str(&spd->process_mask));
       mu_get_quoted_mask(&spd->process_mask, serialized_monitor.cpuset_quoted, CPUSET_MAX);

        /* MPI struct type: serialized_monitor_t */
        MPI_Datatype mpi_serialized_monitor_type;
        {
            int count = 4;
            int blocklengths[] = {HOST_NAME_MAX+CPUSET_MAX*2, 2, 1, 4};
            MPI_Aint displacements[] = {
                offsetof(serialized_monitor_t, hostname),
                offsetof(serialized_monitor_t, pid),
                offsetof(serialized_monitor_t, ipc),
                offsetof(serialized_monitor_t, elapsed_time)};
            MPI_Datatype types[] = {MPI_CHAR, MPI_INT, MPI_FLOAT, mpi_int64_type};
            PMPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_serialized_monitor_type);
            PMPI_Type_commit(&mpi_serialized_monitor_type);
        }

        /* Gather data */
        serialized_monitor_t *recvbuf = NULL;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_mpi_size * sizeof(serialized_monitor_t));
        }
        PMPI_Gather(&serialized_monitor, 1, mpi_serialized_monitor_type,
                recvbuf, 1, mpi_serialized_monitor_type,
                0, MPI_COMM_WORLD);

        /* Add records */
        if (_mpi_rank == 0) {
            int rank;
            for (rank=0; rank<_mpi_size; ++rank) {
                verbose(VB_TALP, "Process summary: recording region %s on rank %d",
                        monitor->name, rank);
                talp_output_record_process(
                        monitor->name,
                        rank,
                        recvbuf[rank].pid,
                        recvbuf[rank].num_measurements,
                        recvbuf[rank].hostname,
                        recvbuf[rank].cpuset,
                        recvbuf[rank].cpuset_quoted,
                        recvbuf[rank].elapsed_time,
                        recvbuf[rank].elapsed_computation_time,
                        recvbuf[rank].accumulated_MPI_time,
                        recvbuf[rank].accumulated_computation_time,
                        recvbuf[rank].ipc);
            }
            free(recvbuf);
        }
    }

    /* SUMMARY POP: Compute POP metrics and record */
    if (spd->options.talp_summary & (SUMMARY_POP_METRICS | SUMMARY_POP_RAW)) {
        if (_mpi_rank == 0) {
            verbose(VB_TALP, "TALP summary: gathering region %s", monitor->name);
        }

        int total_cpus;
        int total_nodes;
        int64_t elapsed_time;
        int64_t elapsed_useful;
        int64_t app_sum_useful;
        int64_t node_sum_useful;
        int64_t num_mpi_calls;
        int64_t cycles;
        int64_t instructions;

        /* Obtain the total number of nodes used in this region */
        int local_node_used = 0;
        int i_used_this_node = monitor->num_measurements > 0 ? 1 : 0;
        PMPI_Reduce(&i_used_this_node, &local_node_used,
                1, MPI_INT, MPI_MAX, 0, getNodeComm());
        if (_process_id == 0) {
            PMPI_Reduce(&local_node_used, &total_nodes,
                    1, MPI_INT, MPI_SUM, 0, getInterNodeComm());
        }

        /* Obtain the maximum elapsed time */
        PMPI_Reduce(&monitor->elapsed_time, &elapsed_time,
                1, mpi_int64_type, MPI_MAX, 0, MPI_COMM_WORLD);

        /* Obtain the maximum elapsed useful time */
        PMPI_Reduce(&monitor->elapsed_computation_time, &elapsed_useful,
                1, mpi_int64_type, MPI_MAX, 0, MPI_COMM_WORLD);

        /* Obtain the sum of all computation time */
        PMPI_Reduce(&monitor->accumulated_computation_time, &app_sum_useful,
                1, mpi_int64_type, MPI_SUM, 0, MPI_COMM_WORLD);

        /* Obtain the sum of computation time of the most loaded node */
        int64_t local_node_useful = 0;
        PMPI_Reduce(&monitor->accumulated_computation_time, &local_node_useful,
                1, mpi_int64_type, MPI_SUM, 0, getNodeComm());
        if (_process_id == 0) {
            PMPI_Reduce(&local_node_useful, &node_sum_useful,
                    1, mpi_int64_type, MPI_MAX, 0, getInterNodeComm());
        }

        /* Obtain the total number of CPUs used in all processes */
        talp_info_t *talp_info = spd->talp_info;
        int ncpus = monitor->num_measurements > 0 ? talp_info->ncpus : 0;
        PMPI_Reduce(&ncpus, &total_cpus,
                1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        /* Obtain the total number of MPI calls */
        PMPI_Reduce(&monitor->num_mpi_calls, &num_mpi_calls, 1,
                mpi_int64_type, MPI_SUM, 0, MPI_COMM_WORLD);

# ifdef PAPI_LIB
        /* Obtain the sum of cycles */
        PMPI_Reduce(&monitor->accumulated_cycles, &cycles, 1,
                mpi_int64_type, MPI_SUM, 0, MPI_COMM_WORLD);
        /* Obtain the sum of instructions*/
        PMPI_Reduce(&monitor->accumulated_instructions, &instructions, 1,
                mpi_int64_type, MPI_SUM, 0, MPI_COMM_WORLD);
#else
        cycles = 0;
        instructions = 0;
# endif

        if (_mpi_rank == 0) {
            if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
                if (elapsed_time > 0) {
                    verbose(VB_TALP, "TALP summary: recording region %s", monitor->name);
                    float avg_ipc = cycles ? (float)instructions / cycles : 0.0f;
                    float parallel_efficiency = (float)app_sum_useful / (elapsed_time * total_cpus);
                    float communication_efficiency = (float)elapsed_useful / elapsed_time;
                    float lb = (float)app_sum_useful / (elapsed_useful * total_cpus);
                    float lb_in =
                        (float)(node_sum_useful * total_nodes) / (elapsed_useful * total_cpus);
                    float lb_out = (float)app_sum_useful / (node_sum_useful * total_nodes);
                    talp_output_record_pop_metrics(
                            monitor->name,
                            elapsed_time,
                            avg_ipc,
                            parallel_efficiency,
                            communication_efficiency,
                            lb,
                            lb_in,
                            lb_out);
                } else {
                    verbose(VB_TALP, "TALP summary: recording empty region %s", monitor->name);
                    talp_output_record_pop_metrics(monitor->name,
                            0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                }
            }
            if (spd->options.talp_summary & SUMMARY_POP_RAW) {
                verbose(VB_TALP, "TALP raw summary: recording region %s", monitor->name);
                talp_output_record_pop_raw(
                        monitor->name,
                        total_cpus,
                        total_nodes,
                        _mpi_size,
                        elapsed_time,
                        elapsed_useful,
                        app_sum_useful,
                        node_sum_useful,
                        num_mpi_calls,
                        cycles,
                        instructions);
            }
        }
    }
}
#endif

#ifdef MPI_LIB
/* Gather data from all monitoring regions */
static void monitoring_regions_gather_data(const subprocess_descriptor_t *spd) {

    /* Warn about open regions */
    for (GSList *node = open_regions;
            node != NULL;
            node = node->next) {
        const dlb_monitor_t *monitor = node->data;
        warning("Region %s is still open during MPI_Finalize."
                " Collected data may be incomplete.",
                monitor->name);
    }

    /*** 1: Gather data from MPI monitor ***/
    talp_info_t *talp_info = spd->talp_info;
    gather_monitor_data(spd, &talp_info->mpi_monitor);

    /*** 2: Gather data from custom regions: ***/

    /* Gather recvcounts for each process */
    int nregions = g_tree_nnodes(regions);
    int chars_to_send = nregions * DLB_MONITOR_NAME_MAX;
    int *recvcounts = malloc(_mpi_size * sizeof(int));
    PMPI_Allgather(&chars_to_send, 1, MPI_INT,
            recvcounts, 1, MPI_INT, MPI_COMM_WORLD);

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
        for (GTreeNode *node = g_tree_node_first(regions);
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
                recvbuffer, recvcounts, displs, MPI_CHAR, MPI_COMM_WORLD);

        /* Register all regions. Existing ones will be skipped. */
        for (i=0; i<total_chars; i+=DLB_MONITOR_NAME_MAX) {
            monitoring_region_register(spd, &recvbuffer[i]);
        }

        free(sendbuffer);
        free(recvbuffer);
        free(displs);
    }

    free(recvcounts);

    /* Finally, reduce data */
    for (GTreeNode *node = g_tree_node_first(regions);
            node != NULL;
            node = g_tree_node_next(node)) {
        const dlb_monitor_t *monitor = g_tree_node_value(node);
        gather_monitor_data(spd, monitor);
    }
}
#endif


/*********************************************************************************/
/*    TALP collect functions                                                     */
/*********************************************************************************/

/* Perform MPI collective calls and compute the current POP metrics for the
 * specified monitor. If monitor is NULL, the implicit MPI monitoring region
 * is assumed.
 * Pre-conditions:
 *  - the given monitor must have been registered in all MPI ranks
 *  - pop_metrics is an allocated structure
 */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
#ifdef MPI_LIB
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == DLB_MPI_REGION) {
        monitor = &talp_info->mpi_monitor;
    }

    /* Stop monitor so that metrics are updated */
    bool resume_region = monitoring_region_stop(spd, monitor) == DLB_SUCCESS;

    int64_t elapsed_time;
    int64_t elapsed_useful;
    int64_t app_sum_useful;
    int64_t node_sum_useful;
    int total_cpus;
    int total_nodes;

    /* Obtain the maximum elapsed time */
    PMPI_Allreduce(&monitor->elapsed_time, &elapsed_time,
            1, mpi_int64_type, MPI_MAX, MPI_COMM_WORLD);

    /* Obtain the maximum elapsed useful time */
    PMPI_Allreduce(&monitor->elapsed_computation_time, &elapsed_useful,
            1, mpi_int64_type, MPI_MAX, MPI_COMM_WORLD);

    /* Obtain the sum of all computation time */
    PMPI_Allreduce(&monitor->accumulated_computation_time, &app_sum_useful,
            1, mpi_int64_type, MPI_SUM, MPI_COMM_WORLD);

    /* Obtain the sum of computation time of the most loaded node */
    int64_t local_node_useful = 0;
    PMPI_Reduce(&monitor->accumulated_computation_time, &local_node_useful,
            1, mpi_int64_type, MPI_SUM, 0, getNodeComm());
    if (_process_id == 0) {
        PMPI_Reduce(&local_node_useful, &node_sum_useful,
                1, mpi_int64_type, MPI_MAX, 0, getInterNodeComm());
    }
    PMPI_Bcast(&node_sum_useful, 1, mpi_int64_type, 0, MPI_COMM_WORLD);

    /* Obtain the total number of CPUs used in all processes */
    int ncpus = monitor->num_measurements > 0 ? talp_info->ncpus : 0;
    PMPI_Allreduce(&ncpus, &total_cpus,
            1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    /* Obtain the total number of nodes used in this region */
    int local_node_used = 0;
    int i_used_this_node = monitor->num_measurements > 0 ? 1 : 0;
    PMPI_Reduce(&i_used_this_node, &local_node_used,
            1, MPI_INT, MPI_MAX, 0, getNodeComm());
    if (_process_id == 0) {
        PMPI_Reduce(&local_node_used, &total_nodes,
                1, MPI_INT, MPI_SUM, 0, getInterNodeComm());
    }
    PMPI_Bcast(&total_nodes, 1, mpi_int64_type, 0, MPI_COMM_WORLD);

    /* Initialize structure */
    *pop_metrics = (const dlb_pop_metrics_t) {
        .total_cpus = total_cpus,
        .total_nodes = total_nodes,
        .elapsed_time = elapsed_time,
        .elapsed_useful = elapsed_useful,
        .app_sum_useful = app_sum_useful,
        .node_sum_useful = node_sum_useful,
    };

    snprintf(pop_metrics->name, DLB_MONITOR_NAME_MAX, "%s", monitor->name);

    /* Compute POP metrics only if needed */
    if (elapsed_time > 0) {
        float parallel_efficiency = (float)app_sum_useful / (elapsed_time * total_cpus);
        float communication_efficiency = (float)elapsed_useful / elapsed_time;
        float lb = (float)app_sum_useful / (elapsed_useful * total_cpus);
        float lb_in =
            (float)(node_sum_useful * total_nodes) / (elapsed_useful * total_cpus);
        float lb_out = (float)app_sum_useful / (node_sum_useful * total_nodes);
        pop_metrics->parallel_efficiency = parallel_efficiency;
        pop_metrics->communication_efficiency = communication_efficiency;
        pop_metrics->lb = lb;
        pop_metrics->lb_in = lb_in;
        pop_metrics->lb_out = lb_out;
    }

    /* Resume monitor */
    if (resume_region) {
        monitoring_region_start(spd, monitor);
    }
    return DLB_SUCCESS;
#else
    return DLB_ERR_NOCOMP;
#endif
}

/* Function that may be called from a third-party process to compute
 * node_metrics for a given region */
int talp_query_pop_node_metrics(const char *name, dlb_node_metrics_t *node_metrics) {

    if (name == NULL) {
        name = mpi_region_name;
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
        /* We can compute the elapsed time, only needed to calculate communication
        * efficiency, as the sum of all Useful + MPI times divided by the number
        * of processes (we may ignore the remainder of the division) */
        int64_t elapsed_time = (total_useful_time + total_mpi_time) / processes_per_node;

        float parallel_efficiency = 0.0f;
        float communication_efficiency = 0.0f;
        float load_balance = 0.0f;
        if (total_useful_time + total_mpi_time > 0) {
            parallel_efficiency = (float)total_useful_time / (total_useful_time + total_mpi_time);
            communication_efficiency = (float)max_useful_time / elapsed_time;
            load_balance = ((float)total_useful_time / processes_per_node) / max_useful_time;
        }

        /* Initialize structure */
        *node_metrics = (const dlb_node_metrics_t) {
            .node_id = node_id,
            .processes_per_node = processes_per_node,
            .total_useful_time = total_useful_time,
            .total_mpi_time = total_mpi_time,
            .max_useful_time = max_useful_time,
            .max_mpi_time = max_mpi_time,
            .parallel_efficiency = parallel_efficiency,
            .communication_efficiency = communication_efficiency,
            .load_balance = load_balance,
        };
        snprintf(node_metrics->name, DLB_MONITOR_NAME_MAX, "%s", name);
    } else {
        error = DLB_ERR_NOENT;
    }

    return error;
}

/* Node-collective function to compute node_metrics for a given region */
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {

    talp_info_t *talp_info = spd->talp_info;
    monitor = monitor ? monitor : &talp_info->mpi_monitor;
    monitor_data_t *monitor_data = monitor->_data;

    /* Stop monitor so that metrics are updated */
    bool resume_region = monitoring_region_stop(spd, monitor) == DLB_SUCCESS;

    /* This functionality needs a shared memory, create a temporary one if needed */
    if (!talp_info->have_shmem) {
        shmem_talp__init(spd->options.shm_key, 1);
        shmem_talp__register(spd->id, monitor->name, &monitor_data->node_shared_id);
    }

    /* Update the shared memory with this process' metrics */
    shmem_talp__set_times(monitor_data->node_shared_id,
            monitor->accumulated_MPI_time,
            monitor->accumulated_computation_time);

    /* Perform a node barrier to ensure everyone has updated their metrics */
    node_barrier(spd, NULL);

    /* Compute node metrics for that region name */
    talp_query_pop_node_metrics(monitor->name, node_metrics);

    /* Remove shared memory if it was a temporary one */
    if (!talp_info->have_shmem) {
        shmem_talp__finalize(spd->id);
    }

    /* Resume monitor */
    if (resume_region) {
        monitoring_region_start(spd, monitor);
    }

    return DLB_SUCCESS;
}
