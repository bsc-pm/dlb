/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/debug.h"

void drom_ext_init(void) {
    pm_init();
    options_init();
    debug_init();
    shmem_cpuinfo_ext__init();
    shmem_procinfo_ext__init();
}

void drom_ext_finalize(void) {
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__finalize();
    options_finalize();
}

int drom_ext_getnumcpus(void) {
    return shmem_cpuinfo_ext__getnumcpus();
}

void drom_ext_getpidlist(int *pidlist, int *nelems, int max_len) {
    shmem_procinfo_ext__getpidlist(pidlist, nelems, max_len);
}

int drom_ext_getprocessmask(int pid, cpu_set_t *mask) {
    return shmem_procinfo_ext__getprocessmask(pid, mask);
}

int drom_ext_setprocessmask(int pid, const cpu_set_t *mask) {
    return shmem_procinfo_ext__setprocessmask(pid, mask);
}

int drom_ext_getcpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len) {
    return shmem_procinfo_ext__getcpus(ncpus, steal, cpulist, nelems, max_len);
}

int drom_ext_preinit(int pid, const cpu_set_t *mask, int steal) {
    int error = shmem_procinfo_ext__preinit(pid, mask, steal);
    error = error ? error : shmem_cpuinfo_ext__preinit(pid, mask, steal);
    return error;
}

int drom_ext_postfinalize(int pid, int return_stolen) {
    int error = shmem_procinfo_ext__postfinalize(pid, return_stolen);
    error = error ? error : shmem_cpuinfo_ext__postfinalize(pid);
    return error;
}
