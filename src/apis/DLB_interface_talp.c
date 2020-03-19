/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/* Tracking Application Low-Level Performance */

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

int DLB_TALP_Attach(void) {
    const char *shm_key;
    if (thread_spd && thread_spd->dlb_initialized) {
        shm_key = thread_spd->options.shm_key;
    } else {
        options_t options;
        options_init(&options, NULL);
        shm_key = options.shm_key;
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

int DLB_TALP_MPITimeGet(int process, double* mpi_time){
    shmem_procinfo__getmpitime((pid_t)process, mpi_time);
    return DLB_SUCCESS;
}

int DLB_TALP_CPUTimeGet(int process, double* compute_time){
    shmem_procinfo__getcomptime((pid_t) process, compute_time);
    return DLB_SUCCESS;
}

dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return NULL;
    }
    return monitoring_region_register(name);
}

int DLB_MonitoringRegionReset(dlb_monitor_t *handle){
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_reset(handle);
}

int DLB_MonitoringRegionStart(dlb_monitor_t *handle){
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_start(handle);
}

int DLB_MonitoringRegionStop(dlb_monitor_t *handle){
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_stop(handle);
}

int DLB_MonitoringRegionReport(dlb_monitor_t *handle){
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->talp_info)) {
        return DLB_ERR_NOTALP;
    }
    return monitoring_region_report(handle);
}
