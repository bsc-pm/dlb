/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/talp_output.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_barrier.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>


/* Talp info per spd */
typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;            /* monitor MPI_Init -> MPI_Finalize */
    int64_t         sample_start_time;      /* global start time to update all regions */
    bool            external_profiler;      /* whether to update shmem on every sample */
    bool            use_counters;           /* whether to use just counters or cpu masks */
    int             num_workers;            /* counter for CPUs doing computation */
    int             num_mpi;                /* counter for CPUs doing mpi */
    int             ncpus;                  /* Number of process CPUs */
    cpu_set_t       workers_mask;           /* CPUs doing computation */
    cpu_set_t       mpi_mask;               /* CPUs doing MPI */
} talp_info_t;

/* Private data per monitor */
typedef struct monitor_data_t {
    int     id;
    bool    started;
} monitor_data_t;


static void talp_node_summary_gather_data(const subprocess_descriptor_t *spd);
static void monitoring_region_initialize(dlb_monitor_t *monitor, int id, const char *name);
static void monitoring_regions_finalize_all(const subprocess_descriptor_t *spd);
static void monitoring_regions_update_all(const subprocess_descriptor_t *spd);
static void monitoring_regions_report_all(const subprocess_descriptor_t *spd);
static void monitoring_regions_gather_data(const subprocess_descriptor_t *spd);

#if MPI_LIB
static MPI_Datatype mpi_int64_type;
#endif

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

#if MPI_LIB
static int cmp_pids(const void *pid1, const void *pid2) {
    return *(pid_t*)pid1 - *(pid_t*)pid2;
}
#endif


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (talp_info_t) {
        .external_profiler = spd->options.talp_external_profiler,
    };
    if (pm_get_num_threads() == 0) {
        /* Probably pure MPI, use only 1 CPU */
        talp_info->use_counters = true;
        talp_info->num_workers = 1;
        talp_info->ncpus = 1;
    } else {
        /* OpenMP/OmpSs detected, use process mask as workers mask */
        talp_info->use_counters = false;
        memcpy(&talp_info->workers_mask, &spd->process_mask, sizeof(cpu_set_t));
        talp_info->ncpus = CPU_COUNT(&talp_info->workers_mask);
    }
    fatal_cond(talp_info->ncpus <= 0,
            "TALP was unable to detect number of CPUS. Please, report bug at "
            PACKAGE_BUGREPORT);
    spd->talp_info = talp_info;

    /* Initialize MPI monitor */
    monitoring_region_initialize(&talp_info->mpi_monitor,
            MPI_MONITORING_REGION_ID, "MPI Execution");

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&spd->process_mask));

    /* Initialize MPI type */
#if MPI_LIB
# if MPI_VERSION >= 3
    mpi_int64_type = MPI_INT64_T;
# else
    MPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(int64_t), &mpi_int64_type);
# endif
#endif
}

static void talp_destroy(void) {
    monitoring_regions_finalize_all(thread_spd);
    free(thread_spd->talp_info);
    thread_spd->talp_info = NULL;
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    verbose(VB_TALP, "Finalizing TALP module");

    if (spd->options.talp_summary & SUMMARY_PROCESS
        || spd->options.talp_summary & SUMMARY_POP_METRICS
        || spd->options.talp_summary & SUMMARY_POP_RAW) {
        monitoring_regions_report_all(spd);
    }

    /* Print/write all collected summaries */
    talp_output_finalize(spd->options.talp_output_file);

    if (spd == thread_spd) {
        /* Keep the timers allocated until the program finalizes */
        atexit(talp_destroy);
    } else {
        monitoring_regions_finalize_all(spd);
        free(spd->talp_info);
        spd->talp_info = NULL;
    }
}

/* Start MPI monitoring region */
void talp_mpi_init(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Start MPI region */
        monitoring_region_start(spd, &talp_info->mpi_monitor);
    }
}

/* Stop MPI monitoring region and gather APP data if needed  */
void talp_mpi_finalize(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Stop MPI region */
        monitoring_region_stop(spd, &talp_info->mpi_monitor);

        /* Update shared memory values */
        shmem_procinfo__settimes(spd->id,
                talp_info->mpi_monitor.accumulated_MPI_time,
                talp_info->mpi_monitor.accumulated_computation_time);

        /* Gather data among processes in the node if node summary is enabled */
        if (spd->options.talp_summary & SUMMARY_NODE) {
            talp_node_summary_gather_data(spd);
        }

        /* Gather data among MPIs if any of these summaries is enabled  */
        if (spd->options.talp_summary
                & (SUMMARY_POP_METRICS | SUMMARY_POP_RAW | SUMMARY_PROCESS)) {
            monitoring_regions_gather_data(spd);
        }
    }
}


/*********************************************************************************/
/*    TALP state change functions (update masks, compute sample times)           */
/*********************************************************************************/

/* The following CPU/cpuset functions should only be called with malleable
 * executions where CPU masks have been initialized. On pure MPI executions
 * they should never be called so it's safe to not ask for the use_counters
 * boolean. */

void talp_cpu_enable(const subprocess_descriptor_t *spd, int cpuid) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (!CPU_ISSET(cpuid, &talp_info->workers_mask)) {
            monitoring_regions_update_all(spd);
            CPU_SET(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Enabling CPU %d. New workers mask: %s",
                    cpuid, mu_to_str(&talp_info->workers_mask));
        }
    }
}

void talp_cpuset_enable(const subprocess_descriptor_t *spd, const cpu_set_t *cpu_mask) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (!mu_is_subset(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all(spd);
            CPU_OR(&talp_info->workers_mask, &talp_info->workers_mask, cpu_mask);
        }
    }
}

void talp_cpu_disable(const subprocess_descriptor_t *spd, int cpuid) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (CPU_ISSET(cpuid, &talp_info->workers_mask)) {
            monitoring_regions_update_all(spd);
            CPU_CLR(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Disabling CPU %d. New workers mask: %s",
                    cpuid, mu_to_str(&talp_info->workers_mask));
        }
    }
}

void talp_cpuset_disable(const subprocess_descriptor_t *spd, const cpu_set_t *cpu_mask) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (mu_intersects(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all(spd);
            mu_substract(&talp_info->workers_mask, &talp_info->workers_mask, cpu_mask);
        }
    }
}

void talp_cpuset_set(const subprocess_descriptor_t *spd, const cpu_set_t *cpu_mask) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (!CPU_EQUAL(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all(spd);
            memcpy(&talp_info->workers_mask, cpu_mask, sizeof(cpu_set_t));
        }
    }
}

void talp_in_mpi(const subprocess_descriptor_t *spd) {
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {

        /* Update all monitors */
        monitoring_regions_update_all(spd);

        /* Update counters/masks */
        if (talp_info->use_counters) {
            talp_info->num_workers = 0;
            talp_info->num_mpi = 1;
        } else if (spd->options.lewi) {
            /* Current CPU goes from worker to MPI mask */
            int cpuid = sched_getcpu();
            CPU_SET(cpuid, &talp_info->mpi_mask);
            CPU_CLR(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Inside MPI. New workers mask: %s",
                    mu_to_str(&talp_info->workers_mask));
        } else {
            /* All CPUs go to MPI mask */
            memcpy(&talp_info->mpi_mask, &talp_info->workers_mask, sizeof(cpu_set_t));
            CPU_ZERO(&talp_info->workers_mask);
        }
    }
}

void talp_out_mpi(const subprocess_descriptor_t *spd){
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {

        /* Update all monitors */
        monitoring_regions_update_all(spd);

        /* Update counters/masks */
        if (talp_info->use_counters) {
            talp_info->num_workers = 1;
            talp_info->num_mpi = 0;
        } else if (spd->options.lewi) {
            /* Current CPU goes from MPI to worker */
            int cpuid = sched_getcpu();
            CPU_CLR(cpuid, &talp_info->mpi_mask);
            CPU_SET(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Outside MPI. New workers mask: %s",
                    mu_to_str(&talp_info->workers_mask));
        } else {
            /* All CPUs go to workers mask */
            memcpy(&talp_info->workers_mask, &talp_info->mpi_mask, sizeof(cpu_set_t));
            CPU_ZERO(&talp_info->mpi_mask);
        }
    }
}

static void talp_node_summary_gather_data(const subprocess_descriptor_t *spd) {
#if MPI_LIB
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
    shmem_barrier__barrier();

    if (_process_id == 0) {
        /* Obtain the PID list */
        int max_procs = mu_get_system_size();
        pid_t *pidlist = malloc(max_procs * sizeof(pid_t));
        int nelems;
        shmem_procinfo__getpidlist(pidlist, &nelems, max_procs);
        qsort(pidlist, nelems, sizeof(pid_t), cmp_pids);

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
            if (pidlist[i] != 0) {
                int64_t mpi_time;
                int64_t useful_time;
                shmem_procinfo__gettimes(pidlist[i], &mpi_time, &useful_time);

                /* Save times in local structure */
                node_summary->process_info[i].pid = pidlist[i];
                node_summary->process_info[i].mpi_time = mpi_time;
                node_summary->process_info[i].useful_time = useful_time;

                /* Accumulate total and max values */
                total_mpi_time +=  mpi_time;
                total_useful_time += useful_time;
                if (max_mpi_time < mpi_time) max_mpi_time = mpi_time;
                if (max_useful_time < useful_time) max_useful_time = useful_time;
            }
        }

        /* Save accumulated values */
        node_summary->total_mpi_time = total_mpi_time;
        node_summary->total_useful_time = total_useful_time;
        node_summary->max_mpi_time = max_mpi_time;
        node_summary->max_useful_time = max_useful_time;

        free(pidlist);
    }

    /* Perform a final barrier so that all processes let the _process_id 0 to
     * gather all the data */
    shmem_barrier__barrier();

    /* All main processes from each node send data to rank 0 */
    if (_process_id == 0) {
        verbose(VB_TALP, "Node summary: gathering data");

        /* MPI type: pid_t */
        MPI_Datatype mpi_pid_type;
        MPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(pid_t), &mpi_pid_type);

        /* MPI struct type: process_in_node_record_t */
        MPI_Datatype mpi_process_info_type;
        {
            int count = 2;
            int blocklengths[] = {1, 2};
            MPI_Aint displacements[] = {
                offsetof(process_in_node_record_t, pid),
                offsetof(process_in_node_record_t, mpi_time)};
            MPI_Datatype types[] = {mpi_pid_type, mpi_int64_type};
            MPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_process_info_type);
            MPI_Type_commit(&mpi_process_info_type);
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
            MPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_node_summary_type);
            MPI_Type_commit(&mpi_node_summary_type);
        }

        /* Gather data */
        void *recvbuf = NULL;
        size_t node_summary_size = sizeof(monitor_node_summary_t)
                + sizeof(process_in_node_record_t) * node_summary->nelems;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_num_nodes * node_summary_size);
        }
        MPI_Gather(node_summary, 1, mpi_node_summary_type,
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
#endif
}


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

dlb_monitor_t* monitoring_region_register(const char* name) {
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
        monitoring_region_initialize(monitor, get_new_monitor_id(), monitor_name);
    } else {
        monitoring_region_initialize(monitor, get_new_monitor_id(), name);
    }

    return monitor;
}

static void monitoring_region_initialize(dlb_monitor_t *monitor, int id, const char *name) {
    /* Initialize private monitor data */
    monitor_data_t *monitor_data = malloc(sizeof(monitor_data_t));
    *monitor_data = (const monitor_data_t) {.id = id};

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

int monitoring_region_reset(dlb_monitor_t *monitor) {
    ++(monitor->num_resets);
    monitor->num_measurements = 0;
    monitor->start_time = 0;
    monitor->stop_time = 0;
    monitor->elapsed_time = 0;
    monitor->elapsed_computation_time = 0;
    monitor->accumulated_MPI_time = 0;
    monitor->accumulated_computation_time = 0;

    monitor_data_t *monitor_data = monitor->_data;
    monitor_data->started = false;

    return DLB_SUCCESS;
}

int monitoring_region_start(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor) {
    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->started) {
        /* Update all monitors */
        monitoring_regions_update_all(spd);

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
    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (monitor_data->started) {
        /* Update all monitors */
        monitoring_regions_update_all(spd);

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

int monitoring_region_report(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor) {
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

static void monitoring_regions_update_all(const subprocess_descriptor_t *spd) {

    talp_info_t *talp_info = spd->talp_info;
    bool use_counters = talp_info->use_counters;

    /* Sample ends here, compute duration and begin new sample */
    int64_t now = get_time_in_ns();
    int64_t sample_duration = now - talp_info->sample_start_time;
    talp_info->sample_start_time = now;

    /* Compute elapsed computation time */
    int64_t elapsed_computation_time =
        (use_counters && talp_info->num_workers > 0)
        || (!use_counters && CPU_COUNT(&talp_info->workers_mask)) > 0
                ? sample_duration : 0;

    /* Compute MPI time */
    int64_t mpi_time = sample_duration *
        (use_counters ? talp_info->num_mpi : CPU_COUNT(&talp_info->mpi_mask));

    /* Compute computation time */
    int64_t computation_time = sample_duration *
        (use_counters ? talp_info->num_workers : CPU_COUNT(&talp_info->workers_mask));

    /* Update MPI monitor */
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;
    monitor_data_t *mpi_monitor_data = mpi_monitor->_data;
    if (mpi_monitor_data != NULL
            && mpi_monitor_data->started) {
        mpi_monitor->elapsed_computation_time += elapsed_computation_time;
        mpi_monitor->accumulated_MPI_time += mpi_time;
        mpi_monitor->accumulated_computation_time += computation_time;
        /* Update shared memory only if requested */
        if (talp_info->external_profiler) {
            shmem_procinfo__settimes(spd->id,
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
            monitor->elapsed_computation_time += elapsed_computation_time;
            monitor->accumulated_MPI_time += mpi_time;
            monitor->accumulated_computation_time += computation_time;
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
        } serialized_monitor_t;

        serialized_monitor_t serialized_monitor = (const serialized_monitor_t) {
            .pid = spd->id,
            .num_measurements = monitor->num_measurements,
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
            int count = 3;
            int blocklengths[] = {2, HOST_NAME_MAX+CPUSET_MAX*2, 4};
            MPI_Aint displacements[] = {
                offsetof(serialized_monitor_t, pid),
                offsetof(serialized_monitor_t, hostname),
                offsetof(serialized_monitor_t, elapsed_time)};
            MPI_Datatype types[] = {MPI_INT, MPI_CHAR, mpi_int64_type};
            MPI_Type_create_struct(count, blocklengths, displacements,
                    types, &mpi_serialized_monitor_type);
            MPI_Type_commit(&mpi_serialized_monitor_type);
        }

        /* Gather data */
        serialized_monitor_t *recvbuf = NULL;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_mpi_size * sizeof(serialized_monitor_t));
        }
        MPI_Gather(&serialized_monitor, 1, mpi_serialized_monitor_type,
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
                        recvbuf[rank].accumulated_computation_time);
            }
            free(recvbuf);
        }
    }

    /* SUMMARY POP: Compute POP metrics and record */
    if (spd->options.talp_summary & (SUMMARY_POP_METRICS | SUMMARY_POP_RAW)) {
        int64_t elapsed_time;
        int64_t elapsed_useful;
        int64_t app_sum_useful;
        int64_t node_sum_useful;
        int total_cpus;
        int total_nodes;

        /* Obtain the maximum elapsed time */
        MPI_Reduce(&monitor->elapsed_time, &elapsed_time,
                1, mpi_int64_type, MPI_MAX, 0, MPI_COMM_WORLD);

        /* Obtain the maximum elapsed useful time */
        MPI_Reduce(&monitor->elapsed_computation_time, &elapsed_useful,
                1, mpi_int64_type, MPI_MAX, 0, MPI_COMM_WORLD);

        /* Obtain the sum of all computation time */
        MPI_Reduce(&monitor->accumulated_computation_time, &app_sum_useful,
                1, mpi_int64_type, MPI_SUM, 0, MPI_COMM_WORLD);

        /* Obtain the sum of computation time of the most loaded node */
        int64_t local_node_useful = 0;
        MPI_Reduce(&monitor->accumulated_computation_time, &local_node_useful,
                1, mpi_int64_type, MPI_SUM, 0, getNodeComm());
        if (_process_id == 0) {
            MPI_Reduce(&local_node_useful, &node_sum_useful,
                    1, mpi_int64_type, MPI_MAX, 0, getInterNodeComm());
        }

        /* Obtain the total number of CPUs used in all processes */
        talp_info_t *talp_info = spd->talp_info;
        int ncpus = monitor->num_measurements > 0 ? talp_info->ncpus : 0;
        MPI_Reduce(&ncpus, &total_cpus,
                1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        /* Obtain the total number of nodes used in this region */
        int local_node_used = 0;
        int i_used_this_node = monitor->num_measurements > 0 ? 1 : 0;
        MPI_Reduce(&i_used_this_node, &local_node_used,
                1, MPI_INT, MPI_MAX, 0, getNodeComm());
        if (_process_id == 0) {
            MPI_Reduce(&local_node_used, &total_nodes,
                    1, MPI_INT, MPI_SUM, 0, getInterNodeComm());
        }

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
                        elapsed_time,
                        elapsed_useful,
                        app_sum_useful,
                        node_sum_useful);
            }
        }
    }
}
#endif

/* Gather data from all monitoring regions */
static void monitoring_regions_gather_data(const subprocess_descriptor_t *spd) {
#ifdef MPI_LIB
    /*** 1: Gather data from MPI monitor ***/
    talp_info_t *talp_info = spd->talp_info;
    gather_monitor_data(spd, &talp_info->mpi_monitor);

    /*** 2: Gather data from custom regions: ***/

    /* Gather recvcounts for each process */
    int chars_to_send = nregions * DLB_MONITOR_NAME_MAX;
    int *recvcounts = malloc(_mpi_size * sizeof(int));
    MPI_Allgather(&chars_to_send, 1, MPI_INT,
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
        MPI_Allgatherv(sendbuffer, nregions * DLB_MONITOR_NAME_MAX, MPI_CHAR,
                recvbuffer, recvcounts, displs, MPI_CHAR, MPI_COMM_WORLD);

        /* Register all regions. Existing ones will be skipped. */
        for (i=0; i<total_chars; i+=DLB_MONITOR_NAME_MAX) {
            monitoring_region_register(&recvbuffer[i]);
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
#endif
}
