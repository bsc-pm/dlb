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

/* Tracking Application Live Performance */

#include "apis/dlb_talp.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_talp.h"
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#pragma GCC visibility push(default)

/*********************************************************************************/
/*    TALP                                                                       */
/*********************************************************************************/

int DLB_TALP_Attach(void) {
    spd_enter_dlb(NULL);
    if (!thread_spd->dlb_initialized) {
        set_observer_role(true);
    }
    char shm_key[MAX_OPTION_LENGTH];
    options_parse_entry("--shm-key", &shm_key);
    shmem_cpuinfo_ext__init(shm_key);
    shmem_procinfo_ext__init(shm_key);
    shmem_talp_ext__init(shm_key, 0);
    return DLB_SUCCESS;
}

int DLB_TALP_Detach(void) {
    int error = shmem_cpuinfo_ext__finalize();
    error = error ? error : shmem_procinfo_ext__finalize();
    error = error ? error : shmem_talp_ext__finalize();
    return error;
}

int DLB_TALP_GetNumCPUs(int *ncpus) {
    *ncpus = shmem_cpuinfo_ext__getnumcpus();
    return DLB_SUCCESS;
}

int DLB_TALP_GetPidList(int *pidlist, int *nelems, int max_len) {
    return shmem_procinfo__getpidlist(pidlist, nelems, max_len);
}

int DLB_TALP_GetTimes(int pid, double *mpi_time, double *useful_time) {
    talp_region_list_t region;
    int error = shmem_talp__get_region(&region, pid,
            monitoring_region_get_MPI_region_name());

    if (error == DLB_SUCCESS) {
        *mpi_time = nsecs_to_secs(region.mpi_time);
        *useful_time = nsecs_to_secs(region.useful_time);
    }

    return error;
}

int DLB_TALP_GetNodeTimes(const char *name, dlb_node_times_t *node_times_list,
        int *nelems, int max_len) {
    int shmem_max_regions = shmem_talp__get_max_regions();
    if (max_len > shmem_max_regions) {
        max_len = shmem_max_regions;
    }
    if (name == DLB_MPI_REGION) {
        name = monitoring_region_get_MPI_region_name();
    }
    talp_region_list_t *region_list = malloc(sizeof(talp_region_list_t)*max_len);
    int error = shmem_talp__get_regionlist(region_list, nelems, max_len, name);
    if (error == DLB_SUCCESS) {
        int i;
        for (i=0; i<*nelems; ++i) {
            node_times_list[i] = (const dlb_node_times_t) {
                .pid         = region_list[i].pid,
                .mpi_time    = region_list[i].mpi_time,
                .useful_time = region_list[i].useful_time,
            };
        }
    }
    free(region_list);
    return error;
}

int DLB_TALP_QueryPOPNodeMetrics(const char *name, dlb_node_metrics_t *node_metrics) {
    return talp_query_pop_node_metrics(name, node_metrics);
}


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

const dlb_monitor_t* DLB_MonitoringRegionGetMPIRegion(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return NULL;
    }
    return monitoring_region_get_MPI_region(thread_spd);
}

dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return NULL;
    }
    return monitoring_region_register(thread_spd, name);
}

int DLB_MonitoringRegionReset(dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_reset(thread_spd, handle);
}

int DLB_MonitoringRegionStart(dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_start(thread_spd, handle);
}

int DLB_MonitoringRegionStop(dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_stop(thread_spd, handle);
}

int DLB_MonitoringRegionReport(const dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_report(thread_spd, handle);
}

int DLB_MonitoringRegionsUpdate(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_regions_force_update(thread_spd);
}

int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return talp_collect_pop_metrics(thread_spd, monitor, pop_metrics);
}

int DLB_TALP_CollectPOPNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    if (unlikely(!thread_spd->options.barrier)) {
        return DLB_ERR_NOCOMP;
    }
    return talp_collect_pop_node_metrics(thread_spd, monitor, node_metrics);
}

int DLB_TALP_CollectNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics)
    __attribute__((alias("DLB_TALP_CollectPOPNodeMetrics")));
