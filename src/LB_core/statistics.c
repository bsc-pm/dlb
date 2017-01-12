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

#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/debug.h"

void stats_ext_init(void) {
    pm_init();
    options_init();
    debug_init();
    shmem_cpuinfo_ext__init();
    shmem_procinfo_ext__init();
}

void stats_ext_finalize(void) {
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__finalize();
    options_finalize();
}

int stats_ext_getnumcpus(void) {
    return shmem_cpuinfo_ext__getnumcpus();
}

void stats_ext_getpidlist(int *pidlist, int *nelems, int max_len) {
    shmem_procinfo_ext__getpidlist(pidlist, nelems, max_len);
}

double stats_ext_getcpuusage(int pid) {
    return shmem_procinfo_ext__getcpuusage(pid);
}

double stats_ext_getcpuavgusage(int pid) {
    return shmem_procinfo_ext__getcpuavgusage(pid);
}

void stats_ext_getcpuusage_list(double *usagelist, int *nelems, int max_len) {
    shmem_procinfo_ext__getcpuusage_list(usagelist, nelems, max_len);
}

void stats_ext_getcpuavgusage_list(double *avgusagelist, int *nelems, int max_len) {
    shmem_procinfo_ext__getcpuavgusage_list(avgusagelist, nelems, max_len);
}

double stats_ext_getnodeusage(void) {
    return shmem_procinfo_ext__getnodeusage();
}

double stats_ext_getnodeavgusage(void) {
    return shmem_procinfo_ext__getnodeavgusage();
}

int stats_ext_getactivecpus(int pid) {
    return shmem_procinfo_ext__getactivecpus(pid);
}

void stats_ext_getactivecpus_list(int *cpuslist, int *nelems, int max_len) {
    shmem_procinfo_ext__getactivecpus_list(cpuslist, nelems, max_len);
}

int stats_ext_getloadavg(int pid, double *load) {
    return shmem_procinfo_ext__getloadavg(pid, load);
}

float stats_ext_getcpustateidle(int cpu) {
    return shmem_cpuinfo_ext__getcpustate(cpu, STATS_IDLE);
}

float stats_ext_getcpustateowned(int cpu) {
    return shmem_cpuinfo_ext__getcpustate(cpu, STATS_OWNED);
}

float stats_ext_getcpustateguested(int cpu) {
    return shmem_cpuinfo_ext__getcpustate(cpu, STATS_GUESTED);
}
