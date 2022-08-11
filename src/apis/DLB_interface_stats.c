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

/* Statistics API */

#include "apis/dlb_stats.h"

#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/options.h"

#include <stddef.h>

#pragma GCC visibility push(default)

int DLB_Stats_Init(void) {
    char shm_key[MAX_OPTION_LENGTH];
    options_parse_entry("--shm-key", &shm_key);
    shmem_cpuinfo_ext__init(shm_key);
    shmem_procinfo_ext__init(shm_key);
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
    shmem_procinfo__getpidlist(pidlist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuUsage(int pid, double *usage) {
    *usage = shmem_procinfo__getcpuusage(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuAvgUsage(int pid, double *usage) {
    *usage = shmem_procinfo__getcpuavgusage(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuUsageList(double *usagelist, int *nelems, int max_len) {
    shmem_procinfo__getcpuusage_list(usagelist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetCpuAvgUsageList(double *avgusagelist, int *nelems, int max_len) {
    shmem_procinfo__getcpuavgusage_list(avgusagelist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetNodeUsage(double *usage) {
    *usage = shmem_procinfo__getnodeusage();
    return DLB_SUCCESS;
}

int DLB_Stats_GetNodeAvgUsage(double *usage) {
    *usage = shmem_procinfo__getnodeavgusage();
    return DLB_SUCCESS;
}

int DLB_Stats_GetActiveCpus(int pid, int *ncpus) {
    *ncpus = shmem_procinfo__getactivecpus(pid);
    return DLB_SUCCESS;
}

int DLB_Stats_GetActiveCpusList(int *cpuslist, int *nelems, int max_len) {
    shmem_procinfo__getactivecpus_list(cpuslist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_Stats_GetLoadAvg(int pid, double *load) {
    return shmem_procinfo__getloadavg(pid, load);
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
