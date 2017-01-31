
/* Statistics API */

#include "LB_core/dlb_stats.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/error.h"

#pragma GCC visibility push(default)

int DLB_Stats_Init(void) {
    shmem_cpuinfo_ext__init();
    shmem_procinfo_ext__init();
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
