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

#ifndef SHMEM_CPUINFO_H
#define SHMEM_CPUINFO_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <limits.h>
#include "support/types.h"

#define CPUINFO_RECOVER_ALL INT_MAX

// Simplified states to keep statistics
typedef enum {
    STATS_IDLE = 0,
    STATS_OWNED,
    STATS_GUESTED,
    _NUM_STATS
} stats_state_t;

void shmem_cpuinfo__init(const cpu_set_t *process_mask, const cpu_set_t *dlb_mask,
        const char *shmem_key);
void shmem_cpuinfo__finalize(void);
int shmem_cpuinfo_ext__preregister(int pid, const cpu_set_t *mask, int steal);
void shmem_cpuinfo__add_mask(const cpu_set_t *cpu_mask);
void shmem_cpuinfo__add_cpu(int cpu);
void shmem_cpuinfo__recover_some_cpus(cpu_set_t *mask, int max_resources);
void shmem_cpuinfo__recover_cpu(int cpu);
int shmem_cpuinfo__return_claimed(cpu_set_t *mask);
int shmem_cpuinfo__collect_mask(cpu_set_t *mask, int max_resources, priority_t priority);
bool shmem_cpuinfo__is_cpu_borrowed(int cpu);
bool shmem_cpuinfo__is_cpu_claimed(int cpu);
int shmem_cpuinfo__reset_default_cpus(cpu_set_t *mask);
bool shmem_cpuinfo__acquire_cpu(int cpu, bool force);
void shmem_cpuinfo__update_ownership(const cpu_set_t* process_mask);

void shmem_cpuinfo_ext__init(const char *shmem_key);
void shmem_cpuinfo_ext__finalize(void);
int shmem_cpuinfo_ext__getnumcpus(void);
float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state);
void shmem_cpuinfo_ext__print_info(bool statistics);
#endif /* SHMEM_CPUINFO_H */
