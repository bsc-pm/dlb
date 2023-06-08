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

#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/talp_output.h"
#include "LB_comm/shmem_barrier.h"
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
static void monitoring_region_initialize(dlb_monitor_t *monitor, int id,
        const char *name, pid_t pid);
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

#if MPI_LIB
static int cmp_regions(const void *region1, const void *region2) {
    const dlb_monitor_t* monitor1 = *((const dlb_monitor_t**)region1);
    const dlb_monitor_t* monitor2 = *((const dlb_monitor_t**)region2);
    return strcmp(monitor1->name, monitor2->name);
}
#endif


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .external_profiler = spd->options.talp_external_profiler,
        .samples_mutex = PTHREAD_MUTEX_INITIALIZER,
    };
    spd->talp_info = talp_info;

    /* Initialize shared memory */
    shmem_talp__init(spd->options.shm_key, spd->options.talp_regions_per_proc);

    /* Initialize MPI monitor */
    monitoring_region_initialize(&talp_info->mpi_monitor,
            MPI_MONITORING_REGION_ID, mpi_region_name, spd->id);

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&spd->process_mask));

    /* Initialize and start running PAPI */
#ifdef PAPI_LIB
    /* Library init */
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) warning("PAPI Library versions differ");

    /* Activate thread tracing */
    int error = PAPI_thread_init(pthread_self);
    error += PAPI_register_thread();

    /* Eventset creation */
    error += PAPI_create_eventset(&EventSet);

    int Events[2] = {PAPI_TOT_CYC, PAPI_TOT_INS};
    error += PAPI_add_events(EventSet, Events, 2);

    /* Start tracing  */
    error += PAPI_start(EventSet);
    if (error != PAPI_OK) warning("PAPI Error during initialization");
#endif
}

static void talp_destroy(void) {
    monitoring_regions_finalize_all(thread_spd);
    free(thread_spd->talp_info);
    thread_spd->talp_info = NULL;
#ifdef PAPI_LIB
    PAPI_shutdown();
#endif
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
    shmem_talp__finalize(spd->id);

    if (spd == thread_spd) {
        /* Keep the timers allocated until the program finalizes */
        atexit(talp_destroy);
    } else {
        monitoring_regions_finalize_all(spd);
        free(spd->talp_info);
        spd->talp_info = NULL;
    }
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
        void *p = realloc(talp_info->samples, sizeof(talp_sample_t*)*ncpus);
        if (p) {
            talp_info->samples = p;
            talp_info->samples[ncpus-1] = malloc(sizeof(talp_sample_t));
            _tls_sample = talp_info->samples[ncpus-1];
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
static void talp_update_sample(const subprocess_descriptor_t *spd, talp_sample_t *sample) {
    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_timestamp */
    int64_t now = get_time_in_ns();

#ifdef PAPI_LIB
    /* Stop counting and store papi_values */
    long long papi_values[2];
    int error = PAPI_stop(EventSet, papi_values);
    if (error != PAPI_OK) verbose(VB_TALP, "stop return code: %d, %s", error, PAPI_strerror(error));
#endif

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
    /* Atomically add papi_values to structure */
    DLB_ATOMIC_ADD_RLX(&sample->cycles, papi_values[0]);
    DLB_ATOMIC_ADD_RLX(&sample->instructions, papi_values[1]);

    /* Reset counters and restart counting */
    error = PAPI_reset(EventSet);
    if (error != PAPI_OK) verbose(VB_TALP, "Error resetting counters");

    error = PAPI_start(EventSet);
    if (error != PAPI_OK) verbose(VB_TALP, "Error starting counting counters again");
#endif

}

/* Accumulate values from samples of all threads and update regions */
static void talp_gather_samples(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    talp_sample_t *thread_sample = talp_get_thread_sample(spd);

    /* Update thread sample with the last microsample */
    talp_update_sample(spd, thread_sample);

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
        monitor_data_t *monitor_data = talp_info->mpi_monitor._data;
        shmem_talp__set_times(monitor_data->node_shared_id,
                talp_info->mpi_monitor.accumulated_MPI_time,
                talp_info->mpi_monitor.accumulated_computation_time);

#ifdef MPI_LIB
        /* If performing any kind of TALP summary, check that the number of processes
         * registered in the shared memory matches with the number of MPI processes in the node.
         * This check is needed to avoid deadlocks on finalize. */
        if (spd->options.talp_summary) {
            if (shmem_barrier__get_num_participants(spd->options.barrier_id) == _mpis_per_node) {

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
                shmem_barrier__barrier(spd->options.barrier_id);
            } else {
                warning("The number of MPI processes and processes registered in DLB differ."
                        " TALP will not print any summary.");
            }
        }
#endif
    }
}

void talp_in_mpi(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_sample_t *sample = talp_get_thread_sample(spd);
        if (!is_blocking_collective || !talp_info->external_profiler) {
            /* Either this MPI call is not a blocking collective or the
             * external_profiler is disabled: just update the sample */
            talp_update_sample(spd, sample);
        } else {
            /* Otherwise, gather samples and update all monitoring regions */
            talp_gather_samples(spd);
        }
        sample->in_useful = false;
    }
}

void talp_out_mpi(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->num_mpi_calls, 1);
        if (!is_blocking_collective || !talp_info->external_profiler) {
            /* Either this MPI call is not a blocking collective or the
             * external_profiler is disabled: just update the sample */
            talp_update_sample(spd, sample);
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
        process_in_node_record_t process_info[0];
    } monitor_node_summary_t;

    monitor_node_summary_t *node_summary = NULL;

    /* Perform a barrier so that all processes in the node have arrived at the
     * MPI_Finalize */
    shmem_barrier__barrier(spd->options.barrier_id);

    if (_process_id == 0) {
        /* Obtain a list of regions associated with "MPI Region", sorted by PID */
        int max_procs = mu_get_system_size();
        talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
        int nelems;
        shmem_talp__getregionlist(region_list, &nelems, max_procs, mpi_region_name);

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
    shmem_barrier__barrier(spd->options.barrier_id);

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

static dlb_monitor_t **regions = NULL;
static int nregions = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

const struct dlb_monitor_t* monitoring_region_get_MPI_region(
        const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    return talp_info ? &talp_info->mpi_monitor : NULL;
}

dlb_monitor_t* monitoring_region_register(const subprocess_descriptor_t *spd,
        const char* name) {
    dlb_monitor_t *monitor = NULL;
    bool anonymous_region = (name == NULL || *name == '\0');
    pthread_mutex_lock(&mutex);
    {
        /* Found monitor if already registered */
        if (!anonymous_region) {
            int i;
            for (i=0; i<nregions; ++i) {
                if (strncmp(regions[i]->name, name, DLB_MONITOR_NAME_MAX-1) == 0) {
                    monitor = regions[i];
                    pthread_mutex_unlock(&mutex);
                    return monitor;
                }
            }
        }

        /* Otherwise, create new monitoring region */
        ++nregions;
        void *p = realloc(regions, sizeof(dlb_monitor_t*)*nregions);
        if (p) {
            regions = p;
            regions[nregions-1] = malloc(sizeof(dlb_monitor_t));
            monitor = regions[nregions-1];
        }
    }
    pthread_mutex_unlock(&mutex);
    fatal_cond(!monitor, "Could not register a new monitoring region."
            " Please report at "PACKAGE_BUGREPORT);

    // Initialize after the mutex is unlocked
    if (anonymous_region) {
        char monitor_name[DLB_MONITOR_NAME_MAX];
        snprintf(monitor_name, DLB_MONITOR_NAME_MAX, "Anonymous Region %d", get_anonymous_id());
        monitoring_region_initialize(monitor, get_new_monitor_id(), monitor_name, spd->id);
    } else {
        monitoring_region_initialize(monitor, get_new_monitor_id(), name, spd->id);
    }

    return monitor;
}

static void monitoring_region_initialize(dlb_monitor_t *monitor,
        int id, const char *name, pid_t pid) {
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
    int err = shmem_talp__register(pid, monitor->name, &monitor_data->node_shared_id);
    if (err < DLB_SUCCESS) {
        warning("Could not register region %s in shared memory, error: %s",
                monitor->name, error_get_str(err));
    }
}

static void monitoring_region_finalize(dlb_monitor_t *monitor) {
    /* Free private data */
    monitor_data_t *monitor_data = monitor->_data;
    free(monitor_data);
    monitor_data = NULL;

    /* Free name */
    free((char*)monitor->name);
    monitor->name = NULL;
}

int monitoring_region_reset(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == NULL) {
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
    if (monitor == NULL) {
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
        monitor_data->started = true;

        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_stop(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    if (monitor == NULL) {
        talp_info_t *talp_info = spd->talp_info;
        monitor = &talp_info->mpi_monitor;
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
        monitor_data->started = false;

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
    if (monitor == NULL) {
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
    monitoring_region_finalize(&talp_info->mpi_monitor);

    /* De-allocate regions */
    pthread_mutex_lock(&mutex);
    {
        int i;
        for (i=0; i<nregions; ++i) {
            monitoring_region_finalize(regions[i]);
            free(regions[i]);
            regions[i] = NULL;
        }
        free(regions);
        regions = NULL;
        nregions = 0;
    }
    pthread_mutex_unlock(&mutex);
}

static void monitoring_regions_update_all(const subprocess_descriptor_t *spd,
        const talp_macrosample_t *macrosample) {
    talp_info_t *talp_info = spd->talp_info;

    /* Update MPI monitor */
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;
    monitor_data_t *mpi_monitor_data = mpi_monitor->_data;
    if (mpi_monitor_data != NULL
            && mpi_monitor_data->started) {
        mpi_monitor->elapsed_computation_time += macrosample->elapsed_useful_time;
        mpi_monitor->accumulated_MPI_time += macrosample->mpi_time;
        mpi_monitor->accumulated_computation_time += macrosample->useful_time;
        mpi_monitor->num_mpi_calls += macrosample->num_mpi_calls;
#ifdef PAPI_LIB
        mpi_monitor->accumulated_cycles += macrosample->cycles;
        mpi_monitor->accumulated_instructions += macrosample->instructions;
#endif
        /* Update shared memory only if requested */
        if (talp_info->external_profiler) {
            shmem_talp__set_times(mpi_monitor_data->node_shared_id,
                    mpi_monitor->accumulated_MPI_time,
                    mpi_monitor->accumulated_computation_time);
        }
    }

    /* Update custom regions */
    int i;
    for (i=0; i<nregions; ++i) {
        dlb_monitor_t *monitor = regions[i];
        monitor_data_t *monitor_data = monitor->_data;
        if (monitor_data->started) {
            monitor->elapsed_computation_time += macrosample->elapsed_useful_time;
            monitor->accumulated_MPI_time += macrosample->mpi_time;
            monitor->accumulated_computation_time += macrosample->useful_time;
            monitor->num_mpi_calls += macrosample->num_mpi_calls;
#ifdef PAPI_LIB
            monitor->accumulated_cycles += macrosample->cycles;
            monitor->accumulated_instructions += macrosample->instructions;
#endif
        }
        /* Update shared memory only if requested */
        if (talp_info->external_profiler) {
            shmem_talp__set_times(monitor_data->node_shared_id,
                    monitor->accumulated_MPI_time,
                    monitor->accumulated_computation_time);
        }
    }
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

        int i;
        for (i=0; i<nregions; ++i) {
            monitoring_region_report(spd, regions[i]);
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
static void gather_monitor_data(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {

    /* Note: we may be sending monitors info twice for PROCESS and POP reports
     * but reducing by MPI communicators makes the implementation much easier
     * and we don't expect the PROCESS summary to be very common */

    /* SUMMARY PROCESS: Collect monitor from all processes and record them all */
    if (spd->options.talp_summary & SUMMARY_PROCESS) {
        /* Each process creates a serialized monitor to send to Rank 0 */
        enum { CPUSET_MAX = 64 };
        typedef struct serialized_monitor_t {
            pid_t   pid;
            int     num_measurements;
            char    hostname[HOST_NAME_MAX];
            char    cpuset[CPUSET_MAX];
            char    cpuset_quoted[CPUSET_MAX];
            int64_t elapsed_time;
            int64_t elapsed_computation_time;
            int64_t accumulated_MPI_time;
            int64_t accumulated_computation_time;
            float   ipc;
        } serialized_monitor_t;

        float ipc = monitor->accumulated_cycles == 0 ? 0.0f
            : (float)monitor->accumulated_instructions / monitor->accumulated_cycles;

        serialized_monitor_t serialized_monitor = (const serialized_monitor_t) {
            .pid = spd->id,
            .num_measurements = monitor->num_measurements,
            .elapsed_time = monitor->elapsed_time,
            .elapsed_computation_time = monitor->elapsed_computation_time,
            .accumulated_MPI_time = monitor->accumulated_MPI_time,
            .accumulated_computation_time = monitor->accumulated_computation_time,
            .ipc = ipc,
        };

       gethostname(serialized_monitor.hostname, HOST_NAME_MAX);
       snprintf(serialized_monitor.cpuset, CPUSET_MAX, "%s", mu_to_str(&spd->process_mask));
       mu_get_quoted_mask(&spd->process_mask, serialized_monitor.cpuset_quoted, CPUSET_MAX);

        /* MPI struct type: serialized_monitor_t */
        MPI_Datatype mpi_serialized_monitor_type;
        {
            int count = 4;
            int blocklengths[] = {2, HOST_NAME_MAX+CPUSET_MAX*2, 4, 1};
            MPI_Aint displacements[] = {
                offsetof(serialized_monitor_t, pid),
                offsetof(serialized_monitor_t, hostname),
                offsetof(serialized_monitor_t, elapsed_time),
                offsetof(serialized_monitor_t, ipc)};
            MPI_Datatype types[] = {MPI_INT, MPI_CHAR, mpi_int64_type, MPI_FLOAT};
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
                    float parallel_efficiency = (float)app_sum_useful / (elapsed_time * total_cpus);
                    float communication_efficiency = (float)elapsed_useful / elapsed_time;
                    float lb = (float)app_sum_useful / (elapsed_useful * total_cpus);
                    float lb_in =
                        (float)(node_sum_useful * total_nodes) / (elapsed_useful * total_cpus);
                    float lb_out = (float)app_sum_useful / (node_sum_useful * total_nodes);
                    talp_output_record_pop_metrics(
                            monitor->name,
                            elapsed_time,
                            parallel_efficiency,
                            communication_efficiency,
                            lb,
                            lb_in,
                            lb_out);
                } else {
                    talp_output_record_pop_metrics(monitor->name, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                }
            }
            if (spd->options.talp_summary & SUMMARY_POP_RAW) {
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
    /*** 1: Gather data from MPI monitor ***/
    talp_info_t *talp_info = spd->talp_info;
    gather_monitor_data(spd, &talp_info->mpi_monitor);

    /*** 2: Gather data from custom regions: ***/

    /* Gather recvcounts for each process */
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
        for (i=0; i<nregions; ++i) {
            strcpy(&sendbuffer[i*DLB_MONITOR_NAME_MAX], regions[i]->name);
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

    /* Each monitoring region will be reduced by alphabetical order */
    pthread_mutex_lock(&mutex);
    qsort(regions, nregions, sizeof(dlb_monitor_t*), cmp_regions);
    pthread_mutex_unlock(&mutex);

    /* Finally, reduce data */
    for (i=0; i<nregions; ++i) {
        gather_monitor_data(spd, regions[i]);
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
    if (monitor == NULL) {
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
#endif
    return DLB_SUCCESS;
}

int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == NULL) {
        monitor = &talp_info->mpi_monitor;
    }

    int64_t total_mpi_time = 0;
    int64_t total_useful_time = 0;
    int64_t max_mpi_time = 0;
    int64_t max_useful_time = 0;

    /* Stop monitor so that metrics are updated */
    bool resume_region = monitoring_region_stop(spd, monitor) == DLB_SUCCESS;

    /* Update the shared memory with this process' metrics */
    monitor_data_t *monitor_data = monitor->_data;
    shmem_talp__set_times(monitor_data->node_shared_id,
            monitor->accumulated_MPI_time,
            monitor->accumulated_computation_time);

    /* Perform a node barrier to ensure everyone has updated their metrics */
    shmem_barrier__barrier(spd->options.barrier_id);

    /* Obtain a list of regions in the node associated with given region */
    int max_procs = mu_get_system_size();
    talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
    int nelems;
    shmem_talp__getregionlist(region_list, &nelems, max_procs, monitor->name);

    /* Iterate the PID list and gather times of every process */
    int i;
    for (i = 0; i <nelems; ++i) {
        int64_t mpi_time = region_list[i].mpi_time;
        int64_t useful_time = region_list[i].useful_time;

        /* Accumulate total and max values */
        total_mpi_time += mpi_time;
        total_useful_time += useful_time;
        max_mpi_time = max_int64(mpi_time, max_mpi_time);
        max_useful_time = max_int64(useful_time, max_useful_time);
    }
    free(region_list);

#if MPI_LIB
    int node_id = _node_id;
#else
    int node_id = 0;
#endif

    /* We can compute the elapsed time, only needed to calculate communication
     * efficiency, as the sum of all Useful + MPI times divided by the number
     * of processes (we may ignore the remainder of the division) */
    int64_t elapsed_time = (total_useful_time + total_mpi_time) / nelems;

    /* Initialize structure */
    *node_metrics = (const dlb_node_metrics_t) {
        .node_id = node_id,
        .processes_per_node = nelems,
        .total_useful_time = total_useful_time,
        .total_mpi_time = total_mpi_time,
        .max_useful_time = max_useful_time,
        .max_mpi_time = max_mpi_time,
        .parallel_efficiency = (float)total_useful_time / (total_useful_time + total_mpi_time),
        .communication_efficiency = (float)max_useful_time / elapsed_time,
        .load_balance = ((float)total_useful_time / nelems) / max_useful_time,
    };

    /* Resume monitor */
    if (resume_region) {
        monitoring_region_start(spd, monitor);
    }

    return DLB_SUCCESS;
}
