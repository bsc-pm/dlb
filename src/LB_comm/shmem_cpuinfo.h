/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#ifndef SHMEM_CPUINFO_H
#define SHMEM_CPUINFO_H

#include "apis/dlb_types.h"
#include "support/types.h"

#include <sys/types.h>
#include <sched.h>


/* Init */
int shmem_cpuinfo__init(pid_t pid, const cpu_set_t *process_mask, const char *shmem_key);
int shmem_cpuinfo_ext__init(const char *shmem_key);
int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags);

/* Finalize */
int shmem_cpuinfo__finalize(pid_t pid, const char *shmem_key);
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
        int64_t *last_borrow, int ncpus, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__acquire_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[],
        pid_t victims[]);

/* Borrow */
int shmem_cpuinfo__borrow_all(pid_t pid, priority_t priority, int *cpus_priority_array,
        int64_t *last_borrow, pid_t new_guests[]);
int shmem_cpuinfo__borrow_cpu(pid_t pid, int cpuid, pid_t *new_guest);
int shmem_cpuinfo__borrow_cpus(pid_t pid, priority_t priority, int *cpus_priority_array,
        int64_t *last_borrow, int ncpus, pid_t new_guests[]);
int shmem_cpuinfo__borrow_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]);

/* Return */
int shmem_cpuinfo__return_all(pid_t pid, pid_t new_guests[]);
int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, pid_t *new_guest);
int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]);

/* Others */
int shmem_cpuinfo__deregister(pid_t pid, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__reset(pid_t pid, pid_t new_guests[], pid_t victims[]);
int shmem_cpuinfo__update_max_parallelism(pid_t pid, int max, pid_t new_guests[], pid_t victims[]);
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t *process_mask);
int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num);
int shmem_cpuinfo__check_cpu_availability(pid_t pid, int cpu);
bool shmem_cpuinfo__exists(void);
void shmem_cpuinfo__enable_request_queues(void);
void shmem_cpuinfo__remove_requests(pid_t pid);
int shmem_cpuinfo__version(void);
size_t shmem_cpuinfo__size(void);

/* WIP: TALP */

void shmem_cpuinfo__print_cpu_times(void);

// Simplified states to keep statistics
typedef enum {
    STATS_IDLE = 0,
    STATS_OWNED,
    STATS_GUESTED,
    _NUM_STATS
} stats_state_t;

int shmem_cpuinfo_ext__getnumcpus(void);
float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state);
void shmem_cpuinfo__print_info(const char *shmem_key, int columns,
        dlb_printshmem_flags_t print_flags);
#endif /* SHMEM_CPUINFO_H */
