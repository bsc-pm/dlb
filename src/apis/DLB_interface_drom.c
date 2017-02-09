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

/* Dynamic Resource Ownership Manager API */

#include "apis/dlb_drom.h"

#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/options.h"

#include <sched.h>

#pragma GCC visibility push(default)

int DLB_DROM_Init(void) {
    const options_t *options = get_global_options();
    shmem_cpuinfo_ext__init(options->shm_key);
    shmem_procinfo_ext__init(options->shm_key);
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
