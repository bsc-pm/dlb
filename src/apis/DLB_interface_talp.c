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
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#pragma GCC visibility push(default)

/*********************************************************************************/
/*    TALP                                                                       */
/*********************************************************************************/

int DLB_TALP_Attach(void) {
    options_t options;
    const char *shm_key;
    if (thread_spd && thread_spd->dlb_initialized) {
        shm_key = thread_spd->options.shm_key;
    } else {
        options_init(&options, NULL);
        shm_key = options.shm_key;
        options_finalize(&options);
    }
    shmem_cpuinfo_ext__init(shm_key);
    shmem_procinfo_ext__init(shm_key);
    return DLB_SUCCESS;
}

int DLB_TALP_Detach(void) {
    int error = shmem_cpuinfo_ext__finalize();
    error = error ? error : shmem_procinfo_ext__finalize();
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
    int64_t mpi_time_ns;
    int64_t useful_time_ns;
    int error = shmem_procinfo__gettimes(pid, &mpi_time_ns, &useful_time_ns);
    if (error == DLB_SUCCESS) {
        *mpi_time = nsecs_to_secs(mpi_time_ns);
        *useful_time = nsecs_to_secs(useful_time_ns);
    }
    return error;
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
    return monitoring_region_register(name);
}

int DLB_MonitoringRegionReset(dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_reset(handle);
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

int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return talp_collect_pop_metrics(thread_spd, monitor, pop_metrics);
}
