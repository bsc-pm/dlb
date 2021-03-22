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
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


/* Talp info per spd */
typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;            /* monitor MPI_Init -> MPI_Finalize */
    cpu_set_t       workers_mask;           /* CPUs doing computation */
    cpu_set_t       mpi_mask;               /* CPUs doing MPI */
    int             ncpus;                  /* Number of process CPUs */
} talp_info_t;

/* Application summary */
typedef struct monitor_app_summary_t {
    int64_t elapsed_time;           /* Elapsed Time */
    int64_t elapsed_useful;         /* Elapsed Useful Computation Time */
    int64_t app_sum_useful;         /* Sum of total Useful Computation Time of all processes */
    int64_t node_sum_useful;        /* Sum of total Useful Computation in the most loaded node */
    int total_cpus;                 /* Sum of total CPUs used (initially registered) by each process */
} monitor_app_summary_t;

/* Private data per monitor */
typedef struct monitor_data_t {
    int     id;
    bool    started;
    int64_t sample_start_time;
    monitor_app_summary_t *app_summary;
} monitor_data_t;


static void talp_node_summary(void);
static void monitoring_region_initialize(dlb_monitor_t *monitor, int id, const char *name);
static void monitoring_regions_finalize_all(void);
static void monitoring_regions_update_all(void);
static void monitoring_regions_report_all(void);
static void monitoring_regions_gather_app_data_all(void);

static inline void sort_pids(pid_t * pidlist, int start, int end){
    int i = start;
    int  nelem = end;
    int j;
    for (i = 0; i < nelem; i++){
        for (j = 0; j < nelem; j++){
            if (pidlist[j] > pidlist[i]){
                int tmp = pidlist[i];
                pidlist[i] = pidlist[j];
                pidlist[j] = tmp;
            }
        }
    }
}

/* Reserved regions ids */
enum { MPI_MONITORING_REGION_ID = 1 };

/* Dynamic region ids */
static int get_new_monitor_id(void) {
    static atomic_int id = MPI_MONITORING_REGION_ID;
    return DLB_ATOMIC_ADD_FETCH_RLX(&id, 1);
}


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (talp_info_t) {};
    memcpy(&talp_info->workers_mask, &spd->process_mask, sizeof(cpu_set_t));
    talp_info->ncpus = CPU_COUNT(&spd->process_mask);
    spd->talp_info = talp_info;

    /* Initialize MPI monitor */
    monitoring_region_initialize(&talp_info->mpi_monitor,
            MPI_MONITORING_REGION_ID, "MPI Execution");

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&talp_info->workers_mask));
}

static void talp_destroy(void) {
    monitoring_regions_finalize_all();
    free(thread_spd->talp_info);
    thread_spd->talp_info = NULL;
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    verbose(VB_TALP, "Finalizing TALP module");

    /* Optional summaries */
    if (thread_spd->options.talp_summary & SUMMARY_NODE) {
        talp_node_summary();
    }
    if (thread_spd->options.talp_summary & SUMMARY_PROCESS
        || thread_spd->options.talp_summary & SUMMARY_REGIONS
        || thread_spd->options.talp_summary & SUMMARY_POP_METRICS) {
        monitoring_regions_report_all();
    }

    if (spd == thread_spd) {
        /* Keep the timers allocated until the program finalizes */
        atexit(talp_destroy);
    } else {
        monitoring_regions_finalize_all();
        free(thread_spd->talp_info);
        thread_spd->talp_info = NULL;
    }
}

/* Start MPI monitoring region */
void talp_mpi_init(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        /* Start MPI region */
        monitoring_region_start(&talp_info->mpi_monitor);
    }
}

/* Stop MPI monitoring region and gather APP data if needed  */
void talp_mpi_finalize(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        /* Stop MPI region */
        monitoring_region_stop(&talp_info->mpi_monitor);

        /* Gather data among MPIs if app summary is enabled  */
        if (thread_spd->options.talp_summary & SUMMARY_POP_METRICS) {
            monitoring_regions_gather_app_data_all();
        }
    }
}

/*********************************************************************************/
/*    Update monitor times based on CPU masks                                    */
/*********************************************************************************/

static void talp_update_monitor(dlb_monitor_t *monitor) {
    talp_info_t *talp_info = thread_spd->talp_info;
    monitor_data_t *monitor_data = monitor->_data;

    /* Compute sample duration */
    int64_t sample_duration = get_time_in_ns() - monitor_data->sample_start_time;

    /* Update elapsed computation time */
    if (CPU_COUNT(&talp_info->workers_mask) > 0) {
        monitor->elapsed_computation_time += sample_duration;
    }

    /* Update MPI time */
    int64_t mpi_time = sample_duration * CPU_COUNT(&talp_info->mpi_mask);
    monitor->accumulated_MPI_time += mpi_time;

    /* Update computation time */
    verbose(VB_TALP, "Updating computation time of region %s with mask: %s (%d)",
            monitor->name, mu_to_str(&talp_info->workers_mask),
            CPU_COUNT(&talp_info->workers_mask));
    int64_t computation_time = sample_duration * CPU_COUNT(&talp_info->workers_mask);
    monitor->accumulated_computation_time += computation_time;

    /* Update shared memory only when updating the main monitor */
    if (monitor == &talp_info->mpi_monitor) {
        shmem_procinfo__settimes(thread_spd->id,
                monitor->accumulated_MPI_time,
                monitor->accumulated_computation_time);
    }

    /* Start new sample */
    monitor_data->sample_start_time = get_time_in_ns();
}


/*********************************************************************************/
/*    TALP state change functions (update masks, compute sample times)           */
/*********************************************************************************/

void talp_cpu_enable(int cpuid) {
    if (unlikely(thread_spd == NULL)) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        if (!CPU_ISSET(cpuid, &talp_info->workers_mask)) {
            monitoring_regions_update_all();
            CPU_SET(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Enabling CPU %d. New workers mask: %s",
                    cpuid, mu_to_str(&talp_info->workers_mask));
        }
    }
}

void talp_cpuset_enable(const cpu_set_t *cpu_mask) {
    if (unlikely(thread_spd == NULL)) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        if (!mu_is_subset(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all();
            CPU_OR(&talp_info->workers_mask, &talp_info->workers_mask, cpu_mask);
        }
    }
}

void talp_cpu_disable(int cpuid) {
    if (unlikely(thread_spd == NULL)) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        if (CPU_ISSET(cpuid, &talp_info->workers_mask)) {
            monitoring_regions_update_all();
            CPU_CLR(cpuid, &talp_info->workers_mask);
            verbose(VB_TALP, "Disabling CPU %d. New workers mask: %s",
                    cpuid, mu_to_str(&talp_info->workers_mask));
        }
    }
}

void talp_cpuset_disable(const cpu_set_t *cpu_mask) {
    if (unlikely(thread_spd == NULL)) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        if (mu_intersects(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all();
            mu_substract(&talp_info->workers_mask, &talp_info->workers_mask, cpu_mask);
        }
    }
}

void talp_cpuset_set(const cpu_set_t *cpu_mask) {
    if (unlikely(thread_spd == NULL)) return;

    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        if (!CPU_EQUAL(cpu_mask, &talp_info->workers_mask)) {
            monitoring_regions_update_all();
            memcpy(&talp_info->workers_mask, cpu_mask, sizeof(cpu_set_t));
        }
    }
}

void talp_in_mpi(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {

        /* Update all monitors */
        monitoring_regions_update_all();

        /* Update masks */
        if (thread_spd->options.lewi) {
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

void talp_out_mpi(void){
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {

        /* Update all monitors */
        monitoring_regions_update_all();

        /* Update masks */
        if (thread_spd->options.lewi) {
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

static void talp_node_summary(void) {
    /* If MPI, print only one rank per node, otherwise, everyone */
#if MPI_LIB
    bool do_print = _process_id == 0;
    int node_id = _node_id;
#else
    bool do_print = true;
    int node_id = 0;
#endif
    if (do_print) {
        int64_t total_mpi_time = 0;
        int64_t total_useful_time = 0;
        int64_t max_mpi_time = 0;
        int64_t max_useful_time = 0;

        int max_procs = mu_get_system_size();
        pid_t *pidlist = malloc(max_procs * sizeof(pid_t));
        int nelems;
        shmem_procinfo__getpidlist(pidlist, &nelems, max_procs);
        sort_pids(pidlist, 0, nelems);

        info(" |----------------------------------------------------------|");
        info(" |                  Extended Report Node %d                 |", node_id);
        info(" |----------------------------------------------------------|");
        info(" |  Process   |     Compute Time     |        MPI Time      |" );
        info(" |------------|----------------------|----------------------|");
        int i;
        for (i = 0; i <nelems; ++i) {
            if (pidlist[i] != 0) {
                int64_t mpi_time;
                int64_t useful_time;
                shmem_procinfo__gettimes(pidlist[i], &mpi_time, &useful_time);
                info(" | %-10d | %18e s | %18e s |", i,
                        nsecs_to_secs(useful_time), nsecs_to_secs(mpi_time));
                info(" |------------|----------------------|----------------------|");

                if( max_mpi_time < mpi_time) max_mpi_time = mpi_time;
                if( max_useful_time < useful_time) max_useful_time = useful_time;

                total_mpi_time +=  mpi_time;
                total_useful_time += useful_time;
            }
        }
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Avg",
                nelems > 0 ? nsecs_to_secs(total_useful_time/nelems) : 0.0,
                nelems > 0 ? nsecs_to_secs(total_mpi_time/nelems) : 0.0);
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Max",
                nsecs_to_secs(max_useful_time),
                nsecs_to_secs(max_mpi_time));
        info(" |------------|----------------------|----------------------|");
    }
}


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

enum { MONITOR_MAX_KEY_LEN = 128 };

static dlb_monitor_t **regions = NULL;
static int nregions = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static const char* anonymous_monitor_name = "Anonymous Region";

const struct dlb_monitor_t* monitoring_region_get_MPI_region(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    return talp_info ? &talp_info->mpi_monitor : NULL;
}

dlb_monitor_t* monitoring_region_register(const char* name){
    dlb_monitor_t *monitor = NULL;
    pthread_mutex_lock(&mutex);
    {
        /* Found monitor if already registered */
        if (name != NULL && *name != '\0') {
            int i;
            for (i=0; i<nregions; ++i) {
                if (strncmp(regions[i]->name, name, MONITOR_MAX_KEY_LEN) == 0) {
                    monitor = regions[i];
                }
            }
        }

        /* Otherwise, create new monitoring region */
        if (monitor == NULL) {
            ++nregions;
            void *p = realloc(regions, sizeof(dlb_monitor_t*)*nregions);
            if (p) {
                regions = p;
                regions[nregions-1] = malloc(sizeof(dlb_monitor_t));
                monitor = regions[nregions-1];
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    fatal_cond(!monitor, "Could not register a new monitoring region."
            " Please report at "PACKAGE_BUGREPORT);

    // Initialize after the mutex is unlocked
    const char *monitor_name;
    if (name != NULL && *name != '\0') {
        monitor_name = name;
    } else {
        monitor_name = anonymous_monitor_name;
    }
    monitoring_region_initialize(monitor, get_new_monitor_id(), monitor_name);

    return monitor;
}

static void monitoring_region_initialize(dlb_monitor_t *monitor, int id, const char *name) {
    /* Initialize opaque data */
    monitor->_data = malloc(sizeof(monitor_data_t));
    monitor_data_t *monitor_data = monitor->_data;

    /* Allocate and assign name */
    monitor->name = strdup(name);

    /* Assign private data */
    monitor_data->id = id;
    monitor_data->app_summary = NULL;

    /* Initialize public data */
    monitoring_region_reset(monitor);
    monitor->num_resets = 0;

    /* Register name in the instrumentation tool */
    instrument_register_event(MONITOR_REGION, monitor_data->id, name);
}

static void monitoring_region_finalize(dlb_monitor_t *monitor) {
    /* Free private data */
    monitor_data_t *monitor_data = monitor->_data;
    if (monitor_data->app_summary != NULL) {
        free(monitor_data->app_summary);
        monitor_data->app_summary = NULL;
    }
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
    monitor_data->sample_start_time = 0;

    return DLB_SUCCESS;
}

int monitoring_region_start(dlb_monitor_t *monitor) {
    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->started) {
        verbose(VB_TALP, "Starting region %s", monitor->name);
        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_BEGIN);

        monitor->start_time = get_time_in_ns();
        monitor->stop_time = 0;
        monitor_data->started = true;
        monitor_data->sample_start_time = monitor->start_time;

        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_stop(dlb_monitor_t *monitor) {
    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (monitor_data->started) {
        verbose(VB_TALP, "Stopping region %s", monitor->name);
        /* Update last sample */
        talp_update_monitor(monitor);

        /* Stop timer */
        monitor->stop_time = get_time_in_ns();
        monitor->elapsed_time += monitor->stop_time - monitor->start_time;
        ++(monitor->num_measurements);
        monitor_data->started = false;
        monitor_data->sample_start_time = 0;

        instrument_event(MONITOR_REGION, monitor_data->id, EVENT_END);
        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_report(const dlb_monitor_t *monitor) {
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
    info("###      CpuSet:  %s", mu_to_str(&thread_spd->process_mask));
    return DLB_SUCCESS;
}

static void monitoring_region_report_pop_metrics(dlb_monitor_t *monitor) {
#ifdef MPI_LIB
    monitor_data_t *monitor_data = monitor->_data;
    monitor_app_summary_t *app_summary = monitor_data->app_summary;
    if (app_summary != NULL) {
        int P = app_summary->total_cpus;
        int N = _num_nodes;
        int64_t elapsed_time = app_summary->elapsed_time;
        int64_t elapsed_useful = app_summary->elapsed_useful;
        int64_t app_sum_useful = app_summary->app_sum_useful;
        int64_t node_sum_useful = app_summary->node_sum_useful;
        float parallel_efficiency = (float)app_sum_useful / (elapsed_time * P);
        float communication_efficiency = (float)elapsed_useful / elapsed_time;
        float lb = (float)app_sum_useful / (elapsed_useful * P);
        float lb_in = (float)(node_sum_useful * N) / (elapsed_useful * P);
        float lb_out = (float)app_sum_useful / (node_sum_useful * N);
        char elapsed_time_str[16];
        ns_to_human(elapsed_time_str, 16, elapsed_time);
        info("######### Monitoring Region App Summary #########");
        info("### Name:                       %s", monitor->name);
        info("### Elapsed Time :              %s", elapsed_time_str);
        info("### Parallel efficiency :       %1.2f", parallel_efficiency);
        info("###   - Communication eff. :    %1.2f", communication_efficiency);
        info("###   - Load Balance :          %1.2f", lb);
        info("###       - LB_in :             %1.2f", lb_in);
        info("###       - LB_out:             %1.2f", lb_out);
    }
#else
    warning("Option --talp-summary=pop-metrics is set but DLB is not intercepting MPI calls.");
    warning("Use a DLB library with MPI support or set --talp-summary=none to hide this warning.");
#endif
}

/* Gather data among all regions and save it in the region private data only on rank 0 */
static void monitoring_region_gather_app_data(dlb_monitor_t *monitor) {
#ifdef MPI_LIB
    int64_t elapsed_time;
    int64_t elapsed_useful;
    int64_t app_sum_useful;
    int64_t node_sum_useful;
    int total_cpus;

    MPI_Datatype mpi_int64;
#if MPI_VERSION >= 3
    mpi_int64 = MPI_INT64_T;
#else
    MPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(int64_t), &mpi_int64);
#endif

    /* Obtain the maximum elapsed time */
    MPI_Reduce(&monitor->elapsed_time, &elapsed_time,
            1, mpi_int64, MPI_MAX, 0, MPI_COMM_WORLD);

    /* Obtain the maximum elapsed useful time */
    MPI_Reduce(&monitor->elapsed_computation_time, &elapsed_useful,
            1, mpi_int64, MPI_MAX, 0, MPI_COMM_WORLD);

    /* Obtain the sum of all computation time */
    MPI_Reduce(&monitor->accumulated_computation_time, &app_sum_useful,
            1, mpi_int64, MPI_SUM, 0, MPI_COMM_WORLD);

    /* Obtain the sum of computation time of the most loaded node */
    int64_t local_node_useful = 0;
    MPI_Reduce(&monitor->accumulated_computation_time, &local_node_useful,
            1, mpi_int64, MPI_SUM, 0, getNodeComm());
    MPI_Reduce(&local_node_useful, &node_sum_useful,
            1, mpi_int64, MPI_MAX, 0, MPI_COMM_WORLD);

    /* Obtain the total number of CPUs used in all processes */
    talp_info_t *talp_info = thread_spd->talp_info;
    MPI_Reduce(&talp_info->ncpus, &total_cpus,
            1, MPI_INTEGER, MPI_SUM, 0, MPI_COMM_WORLD);

    /* Allocate gathered data only in process rank 0 */
    if (_mpi_rank == 0) {
        monitor_data_t *monitor_data = monitor->_data;
        monitor_data->app_summary = malloc(sizeof(monitor_app_summary_t));
        monitor_data->app_summary->elapsed_time = elapsed_time;
        monitor_data->app_summary->elapsed_useful = elapsed_useful;
        monitor_data->app_summary->app_sum_useful = app_sum_useful;
        monitor_data->app_summary->node_sum_useful = node_sum_useful;
        monitor_data->app_summary->total_cpus = total_cpus;
    }
#endif
}

static void monitoring_regions_finalize_all(void) {
    /* Finalize MPI monitor */
    talp_info_t *talp_info = thread_spd->talp_info;
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

static void monitoring_regions_update_all(void) {

    /* Update MPI monitor */
    talp_info_t *talp_info = thread_spd->talp_info;
    monitor_data_t *mpi_monitor_data = talp_info->mpi_monitor._data;
    if (mpi_monitor_data != NULL
            && mpi_monitor_data->started) {
        talp_update_monitor(&talp_info->mpi_monitor);
    }

    /* Update custom regions */
    int i;
    for (i=0; i<nregions; ++i) {
        dlb_monitor_t *monitor = regions[i];
        monitor_data_t *monitor_data = monitor->_data;
        if (monitor_data->started) {
            talp_update_monitor(monitor);
        }
    }
}

static void monitoring_regions_report_all(void) {
    /* Report MPI monitor */
    if (thread_spd->options.talp_summary & SUMMARY_PROCESS) {
        talp_info_t *talp_info = thread_spd->talp_info;
        monitoring_region_report(&talp_info->mpi_monitor);
    }

    /* Report custom regions */
    if (thread_spd->options.talp_summary & SUMMARY_REGIONS) {
        int i;
        for (i=0; i<nregions; ++i) {
            monitoring_region_report(regions[i]);
        }
    }

    /* Report POP Metrics summary */
    if (thread_spd->options.talp_summary & SUMMARY_POP_METRICS) {
        talp_info_t *talp_info = thread_spd->talp_info;
        monitoring_region_report_pop_metrics(&talp_info->mpi_monitor);
        int i;
        for (i=0; i<nregions; ++i) {
            monitoring_region_report_pop_metrics(regions[i]);
        }
    }
}

static void monitoring_regions_gather_app_data_all(void) {
    /* Gather data from MPI monitor */
    talp_info_t *talp_info = thread_spd->talp_info;
    monitoring_region_gather_app_data(&talp_info->mpi_monitor);

    /* Gather data from custom regions */
    int i;
    for (i=0; i<nregions; ++i) {
        monitoring_region_gather_app_data(regions[i]);
    }
}
