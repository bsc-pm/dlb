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

/* Tracking Application Live Performance */

#include "apis/dlb_talp.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "support/dlb_common.h"
#include "support/mask_utils.h"
#include "support/mytime.h"
#include "talp/regions.h"
#include "talp/talp.h"


/*********************************************************************************/
/*    TALP                                                                       */
/*********************************************************************************/

DLB_EXPORT_SYMBOL
int DLB_TALP_Attach(void) {
    int lewi_color;
    char shm_key[MAX_OPTION_LENGTH];
    char *shm_key_ptr;
    int shm_size_multiplier;
    spd_enter_dlb(thread_spd);
    if (!thread_spd->dlb_initialized) {
        set_observer_role(true);
        options_parse_entry("--lewi-color", &lewi_color);
        options_parse_entry("--shm-key", shm_key);
        options_parse_entry("--shm-size-multiplier", &shm_size_multiplier);
        shm_key_ptr = shm_key;
    } else {
        lewi_color = thread_spd->options.lewi_color;
        shm_key_ptr = thread_spd->options.shm_key;
        shm_size_multiplier = thread_spd->options.shm_size_multiplier;
    }
    shmem_cpuinfo_ext__init(shm_key_ptr, lewi_color);
    shmem_procinfo_ext__init(shm_key_ptr, shm_size_multiplier);
    shmem_talp_ext__init(shm_key_ptr, shm_size_multiplier);
    return DLB_SUCCESS;
}

DLB_EXPORT_SYMBOL
int DLB_TALP_Detach(void) {
    int error = shmem_cpuinfo_ext__finalize();
    error = error ? error : shmem_procinfo_ext__finalize();
    error = error ? error : shmem_talp_ext__finalize();
    return error;
}

DLB_EXPORT_SYMBOL
int DLB_TALP_GetNumCPUs(int *ncpus) {
    *ncpus = mu_get_system_size();
    return DLB_SUCCESS;
}

DLB_EXPORT_SYMBOL
int DLB_TALP_GetPidList(int *pidlist, int *nelems, int max_len) {
    return shmem_procinfo__getpidlist(pidlist, nelems, max_len);
}

DLB_EXPORT_SYMBOL
int DLB_TALP_GetTimes(int pid, double *mpi_time, double *useful_time) {

    int error;

    if (pid == 0 || (thread_spd && thread_spd->id == pid)) {
        /* Same process */
        const dlb_monitor_t *monitor = region_get_global(thread_spd);
        if (monitor != NULL) {
            *mpi_time = nsecs_to_secs(monitor->mpi_time);
            *useful_time = nsecs_to_secs(monitor->useful_time);
            error = DLB_SUCCESS;
        } else {
            error = DLB_ERR_NOTALP;
        }
    } else {
        /* Different process, fetch from shared memory */
        talp_region_list_t region;
        error = shmem_talp__get_region(&region, pid, region_get_global_name());

        if (error == DLB_SUCCESS) {
            *mpi_time = nsecs_to_secs(region.mpi_time);
            *useful_time = nsecs_to_secs(region.useful_time);
        }
    }

    return error;
}

DLB_EXPORT_SYMBOL
int DLB_TALP_GetNodeTimes(const char *name, dlb_node_times_t *node_times_list,
        int *nelems, int max_len) {

    int error;

    if (shmem_talp__initialized()) {
        /* Only if a worker process started with --talp-external-profiler */
        int shmem_max_regions = shmem_talp__get_max_regions();
        if (max_len > shmem_max_regions) {
            max_len = shmem_max_regions;
        }
        if (name == DLB_GLOBAL_REGION) {
            name = region_get_global_name();
        }
        talp_region_list_t *region_list = malloc(sizeof(talp_region_list_t)*max_len);
        error = shmem_talp__get_regionlist(region_list, nelems, max_len, name);
        if (error == DLB_SUCCESS) {
            for (int i=0; i<*nelems; ++i) {
                node_times_list[i] = (const dlb_node_times_t) {
                    .pid         = region_list[i].pid,
                    .mpi_time    = region_list[i].mpi_time,
                    .useful_time = region_list[i].useful_time,
                };
            }
        }
        free(region_list);
    } else {
        /* shmem does not exist */
        error = DLB_ERR_NOSHMEM;
    }

    return error;
}

DLB_EXPORT_SYMBOL
int DLB_TALP_QueryPOPNodeMetrics(const char *name, dlb_node_metrics_t *node_metrics) {
    if (shmem_talp__initialized()) {
        /* Only if a worker process started with --talp-external-profiler */
        return talp_query_pop_node_metrics(name, node_metrics);
    } else {
        return DLB_ERR_NOSHMEM;
    }
}


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

DLB_EXPORT_SYMBOL
dlb_monitor_t* DLB_MonitoringRegionGetGlobal(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return NULL;
    }
    return region_get_global(thread_spd);
}

DLB_EXPORT_SYMBOL
DLB_ALIAS(dlb_monitor_t*, DLB_MonitoringRegionGetImplicit, (void), (), \
        DLB_MonitoringRegionGetGlobal)

DLB_EXPORT_SYMBOL
DLB_ALIAS(const dlb_monitor_t*, DLB_MonitoringRegionGetMPIRegion, (void), (), \
        DLB_MonitoringRegionGetGlobal)

DLB_EXPORT_SYMBOL
dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name){
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return NULL;
    }
    return region_register(thread_spd, name);
}

DLB_EXPORT_SYMBOL
int DLB_MonitoringRegionReset(dlb_monitor_t *handle){
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return region_reset(thread_spd, handle);
}

DLB_EXPORT_SYMBOL
int DLB_MonitoringRegionStart(dlb_monitor_t *handle){
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return region_start(thread_spd, handle);
}

DLB_EXPORT_SYMBOL
int DLB_MonitoringRegionStop(dlb_monitor_t *handle){
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return region_stop(thread_spd, handle);
}

DLB_EXPORT_SYMBOL
int DLB_MonitoringRegionReport(const dlb_monitor_t *handle){
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return region_report(thread_spd, handle);
}

DLB_EXPORT_SYMBOL
int DLB_MonitoringRegionsUpdate(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return talp_flush_samples_to_regions(thread_spd);
}

DLB_EXPORT_SYMBOL
int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return talp_collect_pop_metrics(thread_spd, monitor, pop_metrics);
}

DLB_EXPORT_SYMBOL
int DLB_TALP_CollectPOPNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    if (unlikely(!thread_spd->options.barrier)) {
        return DLB_ERR_NOCOMP;
    }
    return talp_collect_pop_node_metrics(thread_spd, monitor, node_metrics);
}

DLB_EXPORT_SYMBOL
DLB_ALIAS(int, DLB_TALP_CollectNodeMetrics, \
        (dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics), \
        (monitor, node_metrics), \
        DLB_TALP_CollectPOPNodeMetrics)
