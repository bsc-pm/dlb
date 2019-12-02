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

#include "apis/dlb.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/DLB_talp.h"

#include "LB_core/spd.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_talp.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#include <string.h>
#include <stdlib.h>

#pragma GCC visibility push(default)

#define SCIENTIFIC_NOTATION 0

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
   // talp_init(thread_spd);
    return DLB_SUCCESS;
}

int DLB_TALP_Detach(void) {
    int error = shmem_cpuinfo_ext__finalize();
    talp_finish(thread_spd);
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
    return monitoring_region_register(name);
}

int DLB_MonitoringRegionStart(dlb_monitor_t handle){
    monitor_info_t * p = (monitor_info_t *) handle;

    return monitoring_region_start(p);
}

int DLB_MonitoringRegionStop(dlb_monitor_t handle){

    monitor_info_t * p = (monitor_info_t *) handle;

    return monitoring_region_stop(p);
}

int DLB_MonitoringRegionReport(dlb_monitor_t handle){
    const subprocess_descriptor_t *spd = thread_spd;
    monitor_info_t * p = (monitor_info_t *) handle;
    const char* cpuset = mu_to_str(&spd->process_mask);
    info("########### Monitoring Regions Summary ##########");
    info("### Name:                      %s", p->name);
#if SCIENTIFIC_NOTATION
    double mpi_time_t=     to_secs( p->mpi_time);
    double compute_time_t= to_secs( p->compute_time);
    info("### MPI time:     %e seconds", mpi_time_t);
    info("### Compute time: %e seconds", compute_time_t);
#else
    info("### Elapsed time :             %lld.%.9ld seconds", (long long) p->elapsed_time.tv_sec, p->elapsed_time.tv_nsec);
    info("### MPI time :                 %lld.%.9ld seconds", (long long) p->mpi_time.tv_sec, p->mpi_time.tv_nsec);
    info("### Compute accumulated time : %lld.%.9ld seconds", (long long) p->compute_time.tv_sec, p->compute_time.tv_nsec);
#endif
    info("###      CpuSet:  %s",cpuset);
    return DLB_SUCCESS;
}

int DLB_MonitoringRegionGet(dlb_monitor_t handle, monitor_info_t *info){
    spd_enter_dlb(NULL);
    return DLB_SUCCESS;
}


