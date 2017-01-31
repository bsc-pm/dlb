
/* Dynamic Resource Ownership Manager API */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include "LB_core/dlb_drom.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/error.h"

#pragma GCC visibility push(default)

int DLB_DROM_Init(void) {
    shmem_cpuinfo_ext__init();
    shmem_procinfo_ext__init();
    return DLB_SUCCESS;
}

int DLB_DROM_Finalize(void) {
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__finalize();
    return DLB_SUCCESS;
}

int DLB_DROM_GetNumCpus(int *ncpus) {
    *ncpus = shmem_cpuinfo_ext__getnumcpus();
    return DLB_SUCCESS;
}

int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len) {
    shmem_procinfo_ext__getpidlist(pidlist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask) {
    return shmem_procinfo_ext__getprocessmask(pid, mask);
}

int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask) {
    return shmem_procinfo_ext__setprocessmask(pid, mask);
}

// Unmaintain?
int DLB_DROM_GetCpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len) {
    return shmem_procinfo_ext__getcpus(ncpus, steal, cpulist, nelems, max_len);
}

int DLB_DROM_PreRegister(int pid, const_dlb_cpu_set_t mask, int steal) {
    int error = shmem_procinfo_ext__preregister(pid, mask, steal);
    error = error ? error : shmem_cpuinfo_ext__preregister(pid, mask, steal);
    return error;
}

#pragma GCC visibility pop
