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


/* Init */
int shmem_cpuinfo__init(pid_t pid, const cpu_set_t *process_mask, const char *shmem_key);
int shmem_cpuinfo_ext__init(const char *shmem_key);
int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, int steal);

/* Finalize */
int shmem_cpuinfo__finalize(pid_t pid);
int shmem_cpuinfo_ext__finalize(void);
int shmem_cpuinfo_ext__postfinalize(pid_t pid);

/* Lend */
int shmem_cpuinfo__lend_cpu(pid_t pid, int cpuid, pid_t *new_guest);
int shmem_cpuinfo__lend_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]);

/* Reclaim */
int shmem_cpuinfo__reclaim_all(pid_t pid, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__reclaim_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim);
int shmem_cpuinfo__reclaim_cpus(pid_t pid, int ncpus, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__reclaim_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[],
        pid_t victims[]);

/* Acquire */
int shmem_cpuinfo__acquire_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim);
int shmem_cpuinfo__acquire_cpus(pid_t pid, priority_t priority, int *cpus_priority_array,
        int ncpus, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__acquire_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[],
        pid_t victims[]);

/* Borrow */
int shmem_cpuinfo__borrow_all(pid_t pid, priority_t priority, int *cpus_priority_array,
        pid_t new_guests[]);
int shmem_cpuinfo__borrow_cpu(pid_t pid, int cpuid, pid_t *new_guest);
int shmem_cpuinfo__borrow_cpus(pid_t pid, priority_t priority, int *cpus_priority_array,
        int ncpus, pid_t new_guests[]);
int shmem_cpuinfo__borrow_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]);

/* Return */
int shmem_cpuinfo__return_all(pid_t pid, pid_t new_guests[]);
int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, pid_t *new_guest);
int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]);

/* Others */
int shmem_cpuinfo__reset(pid_t pid, pid_t new_guests[], pid_t victims[]);
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t *process_mask);
int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num);
bool shmem_cpuinfo__is_cpu_available(pid_t pid, int cpu);
bool shmem_cpuinfo__exists(void);
bool shmem_cpuinfo__is_dirty(void);
void shmem_cpuinfo__enable_request_queues(void);

/* WIP: TALP */
// Simplified states to keep statistics
typedef enum {
    STATS_IDLE = 0,
    STATS_OWNED,
    STATS_GUESTED,
    _NUM_STATS
} stats_state_t;

int shmem_cpuinfo_ext__getnumcpus(void);
float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state);
void shmem_cpuinfo_ext__print_info(bool statistics);
#endif /* SHMEM_CPUINFO_H */
