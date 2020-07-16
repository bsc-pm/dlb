/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_core/DLB_talp.h"

#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
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


typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;            /* monitor MPI_Init -> MPI_Finalize */
    cpu_set_t       workers_mask;           /* CPUs doing computation */
    cpu_set_t       mpi_mask;               /* CPUs doing MPI */
} talp_info_t;


/* Private data per monitor */
typedef struct monitor_data_t {
    bool    started;
    int64_t sample_start_time;
} monitor_data_t;


static void talp_node_summary(void);
static void monitoring_regions_finalize(void);
static void monitoring_regions_update_all(void);
static void monitoring_regions_report_all(void);

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


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    verbose(VB_TALP, "Initializing TALP module");

    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (talp_info_t) {};
    memcpy(&talp_info->workers_mask, &spd->process_mask, sizeof(cpu_set_t));
    spd->talp_info = talp_info;
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    verbose(VB_TALP, "Finalizing TALP module");

    /* Optional summaries */
    if (thread_spd->options.talp_summary & SUMMARY_NODE) {
        talp_node_summary();
    }
    if (thread_spd->options.talp_summary & SUMMARY_PROCESS
        || thread_spd->options.talp_summary & SUMMARY_REGIONS) {
        monitoring_regions_report_all();
    }

    monitoring_regions_finalize();
    free(spd->talp_info);
    spd->talp_info = NULL;
}

/* Start MPI monitoring region */
void talp_mpi_init(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        talp_info->mpi_monitor._data = malloc(sizeof(monitor_data_t));
        monitoring_region_reset(&talp_info->mpi_monitor);
        talp_info->mpi_monitor.num_resets = 0;
        talp_info->mpi_monitor.name = "MPI Execution";
        monitoring_region_start(&talp_info->mpi_monitor);
    }
}

/* Stop MPI monitoring region */
void talp_mpi_finalize(void) {
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        monitoring_region_stop(&talp_info->mpi_monitor);
        free(talp_info->mpi_monitor._data);
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

    /* Update MPI time */
    int64_t mpi_time = sample_duration * CPU_COUNT(&talp_info->mpi_mask);
    monitor->accumulated_MPI_time += mpi_time;

    /* Update computation time */
    int64_t computation_time = sample_duration * CPU_COUNT(&talp_info->workers_mask);
    monitor->accumulated_computation_time += computation_time;

    /* Update shared memory only when updating the main monitor */
    if (monitor == &talp_info->mpi_monitor) {
        shmem_procinfo__setmpitime(thread_spd->id,
                nsecs_to_secs(monitor->accumulated_MPI_time));
        shmem_procinfo__setcomptime(thread_spd->id,
                nsecs_to_secs(monitor->accumulated_computation_time));
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
        double res_mpi = 0;
        double res_comp = 0;
        double max_mpi = 0;
        double max_comp = 0;

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
        double tmp_mpi, tmp_comp;
        for (i = 0; i <nelems; ++i) {
            if (pidlist[i] != 0) {
                shmem_procinfo__getmpitime(pidlist[i], &tmp_mpi);
                shmem_procinfo__getcomptime(pidlist[i], &tmp_comp);
                info(" | %-10d | %18e s | %18e s |", i, tmp_comp, tmp_mpi);
                info(" |------------|----------------------|----------------------|");

                if( max_mpi < tmp_mpi) max_mpi = tmp_mpi;
                if( max_comp < tmp_comp) max_comp = tmp_comp;

                res_mpi +=  tmp_mpi;
                res_comp += tmp_comp;
            }
        }
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Avg", res_comp/nelems, res_mpi/nelems);
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Max", max_comp, max_mpi);
        info(" |------------|----------------------|----------------------|");
    }
}


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

enum { MONITOR_MAX_KEY_LEN = 128 };

static dlb_monitor_t **regions = NULL;
static size_t nregions = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static const char* anonymous_monitor_name = "Anonymous Region";

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

    // Assign new values after the mutex is unlocked
    if (name != NULL && *name != '\0') {
        monitor->name = strdup(name);
    } else {
        monitor->name = anonymous_monitor_name;
    }
    monitor->_data = malloc(sizeof(monitor_data_t));
    monitoring_region_reset(monitor);
    monitor->num_resets = 0;

    return monitor;
}

static void monitoring_regions_finalize(void) {
    /* De-allocate regions */
    pthread_mutex_lock(&mutex);
    {
        int i;
        for (i=0; i<nregions; ++i) {
            dlb_monitor_t *monitor = regions[i];
            if (monitor->name != anonymous_monitor_name) {
                free((char*)monitor->name);
                monitor->name = NULL;
            }
            free(monitor->_data);
            free(monitor);
            monitor = NULL;
        }
        free(regions);
        regions = NULL;
        nregions = 0;
    }
    pthread_mutex_unlock(&mutex);
}

int monitoring_region_reset(dlb_monitor_t *monitor) {
    ++(monitor->num_resets);
    monitor->num_measurements = 0;
    monitor->start_time = 0;
    monitor->stop_time = 0;
    monitor->elapsed_time = 0;
    monitor->accumulated_MPI_time = 0;
    monitor->accumulated_computation_time = 0;
    memset(monitor->_data, 0, sizeof(monitor_data_t));
    return DLB_SUCCESS;
}

int monitoring_region_start(dlb_monitor_t *monitor) {
    int error;
    monitor_data_t *monitor_data = monitor->_data;

    if (!monitor_data->started) {
        instrument_event(MONITOR_REGION, 1, EVENT_BEGIN);

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
        /* Update last sample */
        talp_update_monitor(monitor);

        /* Stop timer */
        monitor->stop_time = get_time_in_ns();
        monitor->elapsed_time += monitor->stop_time - monitor->start_time;
        ++(monitor->num_measurements);
        monitor_data->started = false;
        monitor_data->sample_start_time = 0;

        instrument_event(MONITOR_REGION, 0, EVENT_END);
        error = DLB_SUCCESS;
    } else {
        error = DLB_NOUPDT;
    }

    return error;
}

int monitoring_region_report(dlb_monitor_t *monitor) {
    info("########### Monitoring Region Summary ###########");
    info("### Name:                     %s", monitor->name);
    info("### Elapsed time :            %.9g seconds",
            nsecs_to_secs(monitor->elapsed_time));
    info("### MPI time :                %.9g seconds",
            nsecs_to_secs(monitor->accumulated_MPI_time));
    info("### Computation time :        %.9g seconds",
            nsecs_to_secs(monitor->accumulated_computation_time));
    info("###      CpuSet:  %s", mu_to_str(&thread_spd->process_mask));
    return DLB_SUCCESS;
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
}
