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

#ifndef SHMEM_CPUINFO_H
#define SHMEM_CPUINFO_H

#include "support/types.h"

#include <sys/types.h>
#include <sched.h>
#include <limits.h>

#define CPUINFO_RECOVER_ALL INT_MAX

// Simplified states to keep statistics
typedef enum {
    STATS_IDLE = 0,
    STATS_OWNED,
    STATS_GUESTED,
    _NUM_STATS
} stats_state_t;

int shmem_cpuinfo__init(pid_t pid, const cpu_set_t *process_mask, const char *shmem_key);
int shmem_cpuinfo__finalize(pid_t pid);
int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, int steal);
int shmem_cpuinfo_ext__postfinalize(pid_t pid);

int shmem_cpuinfo__add_cpu(pid_t pid, int cpuid, pid_t *new_pid);
int shmem_cpuinfo__add_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *new_pids);

int shmem_cpuinfo__recover_all(pid_t pid, pid_t *victimlist);
int shmem_cpuinfo__recover_cpu(pid_t pid, int cpuid, pid_t *victim);
int shmem_cpuinfo__recover_cpus(pid_t pid, int ncpus, pid_t *victimlist);
int shmem_cpuinfo__recover_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *victimlist);

int shmem_cpuinfo__collect_all(pid_t pid, pid_t *victimlist);
int shmem_cpuinfo__collect_cpu(pid_t pid, int cpuid, pid_t *victim);
int shmem_cpuinfo__collect_cpus(pid_t pid, int ncpus, pid_t *victimlist);
int shmem_cpuinfo__collect_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *victimlist);

bool shmem_cpuinfo__exists(void);

// old, to be deprecated
int shmem_cpuinfo__collect_mask(pid_t pid, cpu_set_t *mask, int max_resources, priority_t priority);

int shmem_cpuinfo__return_all(pid_t pid, pid_t *new_pids);
int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, pid_t *new_pid);
int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *new_pids);
int shmem_cpuinfo__return_claimed(pid_t pid, cpu_set_t *mask);

bool shmem_cpuinfo__is_cpu_borrowed(pid_t pid, int cpu);
bool shmem_cpuinfo__is_cpu_claimed(pid_t pid, int cpu);
int shmem_cpuinfo__reset_default_cpus(pid_t pid, cpu_set_t *mask);
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t* process_mask);

void shmem_cpuinfo_ext__init(const char *shmem_key);
void shmem_cpuinfo_ext__finalize(void);
int shmem_cpuinfo_ext__getnumcpus(void);
float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state);
void shmem_cpuinfo_ext__print_info(bool statistics);
#endif /* SHMEM_CPUINFO_H */
