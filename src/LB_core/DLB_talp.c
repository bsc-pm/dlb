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

#include "support/debug.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"
#include "apis/dlb.h"

#include <sched.h>
#include <string.h>

#include <unistd.h>
#include "LB_core/DLB_talp.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"

#ifdef MPI_LIB
#include <mpi.h>
#endif

enum { MONITOR_MAX_KEY_LEN = 128 };

static monitor_info_t *regions = NULL;
static size_t nregions;

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

void talp_init( subprocess_descriptor_t *spd ){
    if( spd == NULL || spd->options.talp == 0 || spd->talp_initialized)return;
#ifdef DEBUG
    info("TALP INIT");
    const char* cpuset = mu_to_str(&spd->process_mask);
    info("###      CpuSet:  %s",cpuset);
#endif
    spd->talp_info = malloc(sizeof(talp_info_t));
    talp_info_t * talp_info = (talp_info_t*) (spd->talp_info);
    talp_info->active_working_mask = spd->process_mask;
    CPU_ZERO(&talp_info->in_mpi_mask);
    CPU_ZERO(&talp_info->active_mpi_mask);
    reset(&talp_info->monitoringRegion.mpi_time);
    reset(&talp_info->monitoringRegion.compute_time);
    spd->talp_initialized = 1;
}

void talp_finish( subprocess_descriptor_t *spd ){
    if( spd == NULL || spd->options.talp == 0 || spd->talp_initialized)return;
    spd->talp_initialized = 0;
#ifdef MPI_LIB
    if (_process_id == 0  && spd->options.lewi && spd->options.talp == 0)
    {
#endif
        shmem_cpuinfo__print_cpu_times();
#ifdef MPI_LIB
    }
    talp_mpi_finalize();
    monitoring_regions_finalize();
    free(spd->talp_info);
#endif
}

void talp_cpu_disable_mpi(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if( CPU_ISSET(cpuid,&talp_info->in_mpi_mask)){
        talp_update_monitor(monitoringRegion);
        if( spd->options.lewi_mpi) CPU_CLR(cpuid,&talp_info->active_working_mask);
    }
}

void talp_cpu_disable_working(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if( CPU_ISSET(cpuid,&talp_info->active_working_mask)){
        talp_update_monitor(monitoringRegion);
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
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if(CPU_ISSET(cpuid,&talp_info->in_mpi_mask)){
        talp_update_monitor(monitoringRegion);
        if(spd->options.lewi_mpi){
             CPU_SET(cpuid,&talp_info->active_mpi_mask);
        }
    }
}

void talp_cpu_enable_working(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    if(!CPU_ISSET(cpuid,&talp_info->active_working_mask) ){
        talp_update_monitor(monitoringRegion);
        CPU_SET(cpuid,&talp_info->active_working_mask);
    }
}

void talp_cpu_enable(int cpuid){
    const subprocess_descriptor_t *spd = thread_spd;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    talp_cpu_enable_working(cpuid);
}

void talp_update_monitor(monitor_t* region){
    const subprocess_descriptor_t *spd = thread_spd;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    talp_update_monitor_mpi(region);
    talp_update_monitor_comp(region);
}

void talp_update_monitor_comp(monitor_t* region){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    int pid = spd->id;
    struct timespec aux;
    aux = region->tmp_compute_time;
    if ( clock_gettime(CLOCK_REALTIME, &region->tmp_compute_time) < 0){
       info("DLB ERROR: clock_gettime failed\n");
    }
    diff_time(aux,region->tmp_compute_time,&aux);
#if DEBUG
    info("ELAPSED_TIME: %f",to_secs(aux));
#endif
    mult_time(aux,CPU_COUNT(&talp_info->active_working_mask),&aux);
#if DEBUG
    info("MPI CPUS: %i",CPU_COUNT(&talp_info->active_mpi_mask));
    info("WORKING CPUS: %i",CPU_COUNT(&talp_info->active_working_mask));
    info("COMP ADDED TIME: %f", to_secs(aux));
    info("COMP TIME: %f", to_secs(region->compute_time));
#endif
    add_time(region->compute_time, aux, &region->compute_time);
    shmem_procinfo__setcomptime(pid,to_secs(monitoringRegion->compute_time) );
}

void talp_update_monitor_mpi(monitor_t* region){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    int pid = spd->id;
    struct timespec aux2;
    aux2 = region->tmp_mpi_time;
    if ( clock_gettime(CLOCK_REALTIME, &region->tmp_mpi_time) < 0){
       info("DLB ERROR: clock_gettime failed\n");
    }

    diff_time(aux2,region->tmp_mpi_time,&aux2);
#if DEBUG
    info("ELAPSED_TIME: %f",to_secs(aux2));
#endif
     mult_time(aux2,CPU_COUNT(&talp_info->active_mpi_mask),&aux2);
#if DEBUG
    info("MPI CPUS: %i",CPU_COUNT(&talp_info->active_mpi_mask));
    info("WORKING CPUS: %i",CPU_COUNT(&talp_info->active_working_mask));
    info("MPI ADDED TIME: %f", to_secs(aux2));
    info("MPI TIME: %f", to_secs(region->mpi_time));
#endif
    add_time(region->mpi_time, aux2, &region->mpi_time);
    shmem_procinfo__setmpitime(pid,to_secs(talp_info->monitoringRegion.mpi_time) );
}

void talp_monitor_lock(monitor_t* region){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    pthread_spin_lock(&monitoringRegion->talp_lock);
}

void talp_monitor_unlock(monitor_t* region){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    pthread_spin_unlock(&monitoringRegion->talp_lock);
}

void talp_in_blocking_call(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 )return;
    talp_update_monitor(monitoringRegion);
}

void talp_out_blocking_call(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;

    talp_update_monitor(monitoringRegion);
}

void talp_in_mpi(void){
    int cpunum = sched_getcpu();
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    talp_update_monitor(monitoringRegion);
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
    monitor_t* region =  &((talp_info_t*) (thread_spd->talp_info))->monitoringRegion;
    talp_update_monitor(region);
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

monitor_t* get_talp_monitoringRegion(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if( spd == NULL || spd->talp_info == NULL || spd->options.talp == 0 ) return NULL;
    return monitoringRegion;
}

#ifdef MPI_LIB
void talp_mpi_init(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    if(spd == NULL || !(spd->talp_enabled))return;
    clock_gettime(CLOCK_REALTIME,&talp_info->init_app);
    reset(&monitoringRegion->mpi_time);
    reset(&monitoringRegion->compute_time);
    clock_gettime(CLOCK_REALTIME,&monitoringRegion->tmp_mpi_time);
    clock_gettime(CLOCK_REALTIME,&monitoringRegion->tmp_compute_time);
    clock_gettime(CLOCK_REALTIME,&talp_info->init_app);
}

void talp_mpi_finalize(void){
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
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
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    struct timespec time_aux;
    if(!(spd->talp_enabled))return;
    if( !( thread_spd->options.talp_summary & SUMMARY_PROCESS)) return;

    time_aux = monitoringRegion->tmp_compute_time;
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
#endif

void monitoring_regions_finalize(void) {
    /* Timers Report */
    int i;
    for (i=0; i<nregions && thread_spd->options.talp_summary & SUMMARY_REGIONS; ++i) {
        monitor_info_t *region = &regions[i];
        DLB_MonitoringRegionReport(region);
    }
    /* De-allocate regions */
    free(regions);
    regions= NULL;
    nregions = 0;
}

dlb_monitor_t* monitoring_region_register(const char* name){
    int i;
    for (i=0; i<nregions; ++i) {
        if ((strcmp(regions[i].name, name) == 0)) {
            return (dlb_monitor_t*) &regions[i];
        }
    }
    /* Reallocate new position in timers array */
    ++nregions;
    void *p = realloc(regions, sizeof(monitor_info_t)*nregions);
    if (p) regions = p;
    else fatal("realloc failed");

    monitor_info_t * monitor = (monitor_info_t*) p;

    monitor->name = strdup(name);
    reset(&monitor->compute_time);
    reset(&monitor->mpi_time);

    return (dlb_monitor_t) monitor;
}

int monitoring_region_start(monitor_info_t * p ){
    const subprocess_descriptor_t *spd = thread_spd;
    if(!(spd->talp_enabled))return DLB_ERR_NOTALP;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;

    add_event(MONITOR_REGION,1);
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    p->elapsed_time = now;
    talp_update_monitor(monitoringRegion);

    p->mpi_time = monitoringRegion->mpi_time;
    p->compute_time = monitoringRegion->compute_time;

#if DEBUG
    double mpi_time_t=     to_secs( p->mpi_time);
    double compute_time_t= to_secs( p->compute_time);
    info("########################################");
    info("START MPI time:     %e seconds", mpi_time_t);
    info("START Compute time: %e seconds", compute_time_t);
#endif
    return DLB_SUCCESS;
}

int monitoring_region_stop(monitor_info_t* p){
    const subprocess_descriptor_t *spd = thread_spd;
    if(!(spd->talp_enabled))return DLB_ERR_NOTALP;
    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;
    add_event(MONITOR_REGION,0);
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    talp_update_monitor(monitoringRegion);
    monitoringRegion->tmp_compute_time = now;
    diff_time(p->elapsed_time,now, &p->elapsed_time);
#if DEBUG
    double mpi_time_t=     to_secs(p->mpi_time);
    double compute_time_t= to_secs(p->compute_time);
    double mpi_time_t2=     to_secs(monitoringRegion->mpi_time);
    double compute_time_t2= to_secs(monitoringRegion->compute_time);
#endif
    diff_time(p->mpi_time,monitoringRegion->mpi_time, &p->mpi_time);
    diff_time(p->compute_time,monitoringRegion->compute_time, &p->compute_time);
#if DEBUG
    info("END MPI time:     %e seconds", mpi_time_t);
    info("END MPI2 time:     %e seconds", mpi_time_t2);
    info("END MPI Final:     %e seconds", to_secs(p->mpi_time));
    info("END Compute time: %e seconds", compute_time_t);
    info("END Compute2 time: %e seconds", compute_time_t2);
    info("END Compute2 Final: %e seconds", to_secs(p->compute_time));
#endif
    return DLB_SUCCESS;
}

double talp_get_mpi_time(void){
    const subprocess_descriptor_t *spd = thread_spd;
    if(spd == NULL || !(spd->talp_enabled))return DLB_ERR_NOTALP;

    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;

    return monitoringRegion == NULL ? 0 : to_secs(monitoringRegion->mpi_time);
}

double talp_get_compute_time(void){
    const subprocess_descriptor_t *spd = thread_spd;
    if(spd == NULL || !(spd->talp_enabled))return DLB_ERR_NOTALP;

    talp_info_t * talp_info = (talp_info_t*) spd->talp_info;
    monitor_t * monitoringRegion = &talp_info->monitoringRegion;

    return monitoringRegion == NULL ? 0 : to_secs(monitoringRegion->compute_time);
}
