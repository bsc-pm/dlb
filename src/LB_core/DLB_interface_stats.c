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

/* Statistics API */

#include "LB_core/dlb_stats.h"
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/error.h"
#include "support/options.h"

#pragma GCC visibility push(default)

int DLB_Stats_Init(void) {
    const options_t *options = get_global_options();
    shmem_cpuinfo_ext__init(options->shm_key);
    shmem_procinfo_ext__init(options->shm_key);
    return DLB_SUCCESS;
}

int DLB_Stats_Finalize(void) {
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__finalize();
    return DLB_SUCCESS;
}

int DLB_Stats_GetNumCpus(int *ncpus) {
    *ncpus = shmem_cpuinfo_ext__getnumcpus();
    return DLB_SUCCESS;
}

int DLB_Stats_GetPidList(int *pidlist, int *nelems, int max_len) {
    shmem_procinfo_ext__getpidlist(pidlist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuUsage(int pid, double *usage) {
    *usage = shmem_procinfo_ext__getcpuusage(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuAvgUsage(int pid, double *usage) {
    *usage = shmem_procinfo_ext__getcpuavgusage(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuUsageList(double *usagelist, int *nelems, int max_len) {
    shmem_procinfo_ext__getcpuusage_list(usagelist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuAvgUsageList(double *avgusagelist, int *nelems, int max_len) {
    shmem_procinfo_ext__getcpuavgusage_list(avgusagelist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetNodeUsage(double *usage) {
    *usage = shmem_procinfo_ext__getnodeusage();
    return DLB_SUCCESS;
}

int DLB_Stats_GetNodeAvgUsage(double *usage) {
    *usage = shmem_procinfo_ext__getnodeavgusage();
    return DLB_SUCCESS;
}

int DLB_Stats_GetActiveCpus(int pid, int *ncpus) {
    *ncpus = shmem_procinfo_ext__getactivecpus(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetActiveCpusList(int *cpuslist, int *nelems, int max_len) {
    shmem_procinfo_ext__getactivecpus_list(cpuslist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetLoadAvg(int pid, double *load) {
    return shmem_procinfo_ext__getloadavg(pid, load);
}

int DLB_Stats_GetCpuStateIdle(int cpu, float *percentage) {
    *percentage = shmem_cpuinfo_ext__getcpustate(cpu, STATS_IDLE);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuStateOwned(int cpu, float *percentage) {
    *percentage = shmem_cpuinfo_ext__getcpustate(cpu, STATS_OWNED);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuStateGuested(int cpu, float *percentage) {
    *percentage = shmem_cpuinfo_ext__getcpustate(cpu, STATS_GUESTED);
    return DLB_SUCCESS;
}

#pragma GCC visibility pop
