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

#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef MPI_LIB
#include <mpi.h>
#endif

typedef struct monitor_data_t {
    bool    started;
    int64_t sample_start_time;
} monitor_data_t;


static void monitoring_regions_finalize(void);

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
    memcpy(&talp_info->active_working_mask, &spd->process_mask, sizeof(cpu_set_t));
    spd->talp_info = talp_info;
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    verbose(VB_TALP, "Finalizing TALP module");

#ifdef MPI_LIB
    if (_process_id == 0  && spd->options.lewi) {
#endif
        shmem_cpuinfo__print_cpu_times();
#ifdef MPI_LIB
    }
    talp_mpi_finalize();
#endif
    monitoring_regions_finalize();
    free(spd->talp_info);
    spd->talp_info = NULL;
}


/*********************************************************************************/
/*    Update monitor times based on CPU masks                                    */
/*********************************************************************************/

static void talp_update_monitor(dlb_monitor_t *monitor, int64_t sample_duration) {
    talp_info_t *talp_info = thread_spd->talp_info;

    /* Update MPI time */
    int64_t mpi_time = sample_duration * CPU_COUNT(&talp_info->active_mpi_mask);
    monitor->accumulated_MPI_time += mpi_time;

    /* Update computation time */
    int64_t computation_time = sample_duration * CPU_COUNT(&talp_info->active_working_mask);
    monitor->accumulated_computation_time += computation_time;

    /* Update shared memory only when updating the main monitor */
    if (monitor == &talp_info->monitoringRegion) {
        shmem_procinfo__setmpitime(thread_spd->id,
                nsecs_to_secs(monitor->accumulated_MPI_time));
        shmem_procinfo__setcomptime(thread_spd->id,
                nsecs_to_secs(monitor->accumulated_computation_time));
    }
}


void talp_cpu_disable_mpi(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if( CPU_ISSET(cpuid,&talp_info->in_mpi_mask)){
        talp_update_monitor(monitoringRegion, 0);
        if( spd->options.lewi_mpi) CPU_CLR(cpuid,&talp_info->active_working_mask);
    }
}

void talp_cpu_disable_working(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if( CPU_ISSET(cpuid,&talp_info->active_working_mask)){
        talp_update_monitor(monitoringRegion, 0);
        CPU_CLR(cpuid,&talp_info->active_working_mask);
    }
}

void talp_cpu_disable(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    talp_cpu_disable_working(cpuid);
}

void talp_cpu_enable_mpi(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if(CPU_ISSET(cpuid,&talp_info->in_mpi_mask)){
        talp_update_monitor(monitoringRegion, 0);
        if(spd->options.lewi_mpi){
             CPU_SET(cpuid,&talp_info->active_mpi_mask);
        }
    }
}

void talp_cpu_enable_working(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if(!CPU_ISSET(cpuid,&talp_info->active_working_mask) ){
        talp_update_monitor(monitoringRegion, 0);
        CPU_SET(cpuid,&talp_info->active_working_mask);
    }
}

void talp_cpu_enable(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    talp_cpu_enable_working(cpuid);
}

void talp_in_blocking_call(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 )return;
    talp_update_monitor(monitoringRegion, 0);
}

void talp_out_blocking_call(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;

    talp_update_monitor(monitoringRegion, 0);
}

void talp_in_mpi(void){
    int cpunum = sched_getcpu();
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    talp_update_monitor(monitoringRegion, 0);
    if( thread_spd->options.lewi){
        CPU_SET(cpunum,&talp_info->in_mpi_mask);
        CPU_SET(cpunum,&talp_info->active_mpi_mask);
        CPU_CLR(cpunum,&talp_info->active_working_mask);
    }
    else{
         talp_info->active_mpi_mask = talp_info->active_working_mask;
         talp_info->in_mpi_mask = talp_info->active_working_mask;
         CPU_ZERO(&talp_info->active_working_mask);
    }
}

void talp_out_mpi(void){
    int cpunum = sched_getcpu();
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *region =  &((talp_info_t*) (thread_spd->talp_info))->monitoringRegion;
    talp_update_monitor(region, 0);
    if( spd->options.lewi){
        CPU_CLR(cpunum,&talp_info->in_mpi_mask);
        CPU_CLR(cpunum,&talp_info->active_mpi_mask);
        CPU_SET(cpunum,&talp_info->active_working_mask);
    }
    else{
        talp_info->active_working_mask = talp_info->active_mpi_mask;
        CPU_ZERO(&talp_info->active_mpi_mask);
        CPU_ZERO(&talp_info->in_mpi_mask);
    }
}

talp_info_t * get_talp_global_info(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return NULL;
    return  talp_info;
}

dlb_monitor_t* get_talp_monitoringRegion(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return NULL;
    return monitoringRegion;
}

#ifdef MPI_LIB
void talp_mpi_init(void){
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info) {
        dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
        clock_gettime(CLOCK_REALTIME,&talp_info->init_app);
        reset(&monitoringRegion->mpi_time);
        reset(&monitoringRegion->compute_time);
        clock_gettime(CLOCK_REALTIME,&monitoringRegion->tmp_mpi_time);
        clock_gettime(CLOCK_REALTIME,&monitoringRegion->tmp_compute_time);
        clock_gettime(CLOCK_REALTIME,&talp_info->init_app);
    }
}

void talp_mpi_finalize(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
    int  rank;
    if( ! ( thread_spd->options.talp_summary & SUMMARY_NODE))return;
    int pid = getpid();

    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    clock_gettime(CLOCK_REALTIME,&talp_info->end_app);
    diff_time(talp_info->init_app, talp_info->end_app, &talp_info->end_app);
    int stats_per_node = 1;
    shmem_procinfo__setmpitime(pid,to_secs(monitoringRegion->mpi_time) );
    shmem_procinfo__setcomptime(pid,to_secs(monitoringRegion->compute_time) );

    MPI_Barrier(MPI_COMM_WORLD);

    if ( _process_id == 0){
        double res_mpi = 0;
        double res_comp = 0;
        double max_mpi = 0;
        double max_comp = 0;

        pid_t *pidlist = malloc(100 * sizeof(pid_t));
        int nelem;
#ifndef COMPACT_MODE
        info(" |----------------------------------------------------------|");
        info(" |                  Extended Report Node %i                 |",_node_id);
        info(" |----------------------------------------------------------|");
        info(" |  Process   |     Compute Time     |        MPI Time      |" );
        info(" |------------|----------------------|----------------------|");
        shmem_procinfo__getpidlist(pidlist, &nelem,-1);
        sort_pids(pidlist, 0, nelem);
            int i;
            double tmp_mpi, tmp_comp;
            for( i = 0; i <nelem; ++i){
                if( stats_per_node && pidlist[i] != 0 ){
                    shmem_procinfo__getmpitime(pidlist[i], &tmp_mpi);
                    shmem_procinfo__getcomptime(pidlist[i], &tmp_comp);
                    info(" | %-10i | %18e s | %18e s |", i, tmp_comp, tmp_mpi);
                    info(" |------------|----------------------|----------------------|");

                    if( max_mpi < tmp_mpi) max_mpi = tmp_mpi;
                    if( max_comp < tmp_comp) max_comp = tmp_comp;

                    res_mpi +=  tmp_mpi;
                    res_comp += tmp_comp;
            }
        }
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Avg", res_comp/nelem, res_mpi/nelem);
        info(" |------------|----------------------|----------------------|");
        info(" | %-10s | %18e s | %18e s |", "Node Max", max_comp, max_mpi);
        info(" |------------|----------------------|----------------------|");
#else
        info(" |----------------------------------------------------------|");
        info(" |                  Extended Report Node %i                 |",_node_id);
        info(" |----------------------------------------------------------|");
        info(" |  Process   |     Compute Time     |        MPI Time      |" );
        info(" |----------------------------------------------------------|");
        sort_pids(pidlist, 0, nelem);
        shmem_procinfo__getpidlist(pidlist, &nelem,-1);
        int i;
        double tmp_mpi, tmp_comp;
        for( i = 0; i <nelem; ++i){
            if( stats_per_node && pidlist[i] != 0 ){
                shmem_procinfo__getmpitime(pidlist[i], &tmp_mpi);
                shmem_procinfo__getcomptime(pidlist[i], &tmp_comp);
#ifndef SCIENTIFIC
                info("%i;%.2f;%.2f\n", i, tmp_comp, tmp_mpi);
#else
                info("%i;%e;%e\n", i, tmp_comp, tmp_mpi);
#endif
                if( max_mpi < tmp_mpi) max_mpi = tmp_mpi;
                if( max_comp < tmp_comp) max_comp = tmp_comp;

                res_mpi +=  tmp_mpi;
                res_comp += tmp_comp;
            }
        }
        free(pidlist);
#ifndef SCIENTIFIC
        info("%s;%.2f;%.2f\n", "Node Avg", res_comp/nelem, res_mpi/nelem);
        info("%s;%.2f;%.2f\n", "Node Max", max_comp, max_mpi);
        info("%s;%.2f\n", "AppTime", to_secs(talp_info->end_app));
#else
        info("%s;%e;%e \n", "Node Avg", res_comp/nelem, res_mpi/nelem);
        info("%s;%e;%e \n", "Node Max", max_comp, max_mpi);
        info("%s;%e\n", "AppTime", to_secs(talp_info->end_app));
#endif
#endif
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

void talp_mpi_report(void){
    talp_info_t *talp_info = thread_spd->talp_info;
    if (talp_info &&
            thread_spd->options.talp_summary & SUMMARY_PROCESS) {
        dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;
        struct timespec time_aux = monitoringRegion->tmp_compute_time;
        clock_gettime(CLOCK_REALTIME, &monitoringRegion->tmp_compute_time);

        diff_time(time_aux,monitoringRegion->tmp_compute_time , &time_aux);
        add_time(monitoringRegion->compute_time,time_aux,&monitoringRegion->compute_time);
        double mpi_time_t=     to_secs( monitoringRegion->mpi_time);
        double compute_time_t= to_secs( monitoringRegion->compute_time);
        info("Monitoring Regions Process Summary");
        info("### Name:        %s", "Zone");
        info("### MPI time:     %e seconds", mpi_time_t);
        info("### Compute time: %e seconds", compute_time_t);
    }
}
#endif


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

enum { MONITOR_MAX_KEY_LEN = 128 };

static dlb_monitor_t **regions = NULL;
static size_t nregions = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

dlb_monitor_t* monitoring_region_register(const char* name){
    dlb_monitor_t *monitor = NULL;
    pthread_mutex_lock(&mutex);
    {
        /* Found monitor if already registered */
        int i;
        for (i=0; i<nregions; ++i) {
            if (strncmp(regions[i]->name, name, MONITOR_MAX_KEY_LEN) == 0) {
                monitor = regions[i];
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
    monitor->name = strdup(name);
    monitor->_data = malloc(sizeof(monitor_data_t));
    monitoring_region_reset(monitor);
    monitor->num_resets = 0;

    return monitor;
}

static void monitoring_regions_finalize(void) {
    /* Individual Report */
    int i;
    for (i=0; i<nregions && thread_spd->options.talp_summary & SUMMARY_REGIONS; ++i) {
        monitoring_region_report(regions[i]);
    }

    /* De-allocate regions */
    pthread_mutex_lock(&mutex);
    {
        for (i=0; i<nregions; ++i) {
            dlb_monitor_t *monitor = regions[i];
            free((char*)monitor->name);
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
    monitor->end_time = 0;
    monitor->elapsed_time_ = 0;
    monitor->accumulated_MPI_time = 0;
    monitor->accumulated_computation_time = 0;
    memset(monitor->_data, 0, sizeof(monitor_data_t));
    return DLB_SUCCESS;
}

int monitoring_region_start(dlb_monitor_t *monitor) {
    add_event(MONITOR_REGION,1);

    monitor_data_t *monitor_data = monitor->_data;
    monitor->start_time = get_time_in_ns();
    monitor->end_time = 0;
    monitor_data->started = true;
    monitor_data->sample_start_time = monitor->start_time;

    return DLB_SUCCESS;
}

int monitoring_region_stop(dlb_monitor_t *monitor) {
    monitor_data_t *monitor_data = monitor->_data;

    /* Stop timer */
    monitor->end_time = get_time_in_ns();

    /* Update times */
    monitor->elapsed_time_ += monitor->end_time - monitor->start_time;
    int64_t sample_duration = monitor->end_time - monitor_data->sample_start_time;
    talp_update_monitor(monitor, sample_duration);

    /* Mark it as stopped */
    ++(monitor->num_measurements);
    monitor_data->started = false;
    monitor_data->sample_start_time = 0;

    add_event(MONITOR_REGION,0);
    return DLB_SUCCESS;
}

int monitoring_region_report(dlb_monitor_t *monitor) {
    info("########### Monitoring Regions Summary ##########");
    info("### Name:                      %s", monitor->name);
    info("### Elapsed time :             %.9g seconds",
            nsecs_to_secs(monitor->elapsed_time_));
    info("### MPI time :                 %.9g seconds",
            nsecs_to_secs(monitor->accumulated_MPI_time));
    info("### Compute accumulated time : %.9g seconds",
            nsecs_to_secs(monitor->accumulated_computation_time));
    info("###      CpuSet:  %s", mu_to_str(&thread_spd->process_mask));
    return DLB_SUCCESS;
}

double talp_get_mpi_time(void){
    talp_info_t *talp_info = thread_spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;

    return monitoringRegion == NULL ? 0 : to_secs(monitoringRegion->mpi_time);
}

double talp_get_compute_time(void){
    talp_info_t *talp_info = thread_spd->talp_info;
    dlb_monitor_t *monitoringRegion = &talp_info->monitoringRegion;

    return monitoringRegion == NULL ? 0 : to_secs(monitoringRegion->compute_time);
}
