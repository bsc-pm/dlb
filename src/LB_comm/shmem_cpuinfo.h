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

#ifndef SHMEM_CPUINFO_H
#define SHMEM_CPUINFO_H

#include "apis/dlb_types.h"
#include "support/types.h"

#include <sys/types.h>
#include <sched.h>

/* Packed enum to try to make cpuinfo_task_t size 8 bytes */
typedef enum __attribute__((__packed__)) {
    ENABLE_CPU,
    DISABLE_CPU
} cpuinfo_action_t;

typedef struct cpuinfo_task_t {
    pid_t            pid;
    cpuid_t          cpuid;
    cpuinfo_action_t action;
} cpuinfo_task_t;

typedef struct array_cpuid_t array_cpuid_t;
typedef struct array_cpuinfo_task_t array_cpuinfo_task_t;

/* Init */
int shmem_cpuinfo__init(pid_t pid, pid_t preinit_pid, const cpu_set_t *process_mask,
        const char *shmem_key, int shmem_color);
int shmem_cpuinfo_ext__init(const char *shmem_key, int shmem_color);
int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags);

/* Finalize */
int shmem_cpuinfo__finalize(pid_t pid, const char *shmem_key, int shmem_color);
int shmem_cpuinfo_ext__finalize(void);
int shmem_cpuinfo_ext__postfinalize(pid_t pid);

/* Lend */
int shmem_cpuinfo__lend_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__lend_cpu_mask(pid_t pid, const cpu_set_t *restrict mask,
        array_cpuinfo_task_t *restrict tasks);

/* Reclaim */
int shmem_cpuinfo__reclaim_all(pid_t pid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__reclaim_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__reclaim_cpus(pid_t pid, int ncpus, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__reclaim_cpu_mask(pid_t pid, const cpu_set_t *restrict mask,
        array_cpuinfo_task_t *restrict tasks);

/* Acquire */
int shmem_cpuinfo__acquire_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__acquire_ncpus_from_cpu_subset(
        pid_t pid, int *restrict requested_ncpus,
        const array_cpuid_t *restrict cpus_priority_array,
        lewi_affinity_t lewi_affinity, int max_parallelism,
        int64_t *restrict last_borrow, array_cpuinfo_task_t *restrict tasks);

/* Borrow */
int shmem_cpuinfo__borrow_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__borrow_ncpus_from_cpu_subset(
        pid_t pid, int *restrict requested_ncpus,
        const array_cpuid_t *restrict cpus_priority_array, lewi_affinity_t lewi_affinity,
        int max_parallelism, int64_t *restrict last_borrow,
        array_cpuinfo_task_t *restrict tasks);

/* Return */
int shmem_cpuinfo__return_all(pid_t pid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask,
        array_cpuinfo_task_t *restrict tasks);
void shmem_cpuinfo__return_async_cpu(pid_t pid, cpuid_t cpuid);
void shmem_cpuinfo__return_async_cpu_mask(pid_t pid, const cpu_set_t *mask);

/* Others */
int shmem_cpuinfo__deregister(pid_t pid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__reset(pid_t pid, array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__update_max_parallelism(pid_t pid, unsigned int max,
        array_cpuinfo_task_t *restrict tasks);
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t *restrict process_mask,
        array_cpuinfo_task_t *restrict tasks);
int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num);
int shmem_cpuinfo__get_nth_non_owned_cpu(pid_t pid, int nth_cpu);
int shmem_cpuinfo__get_number_of_non_owned_cpus(pid_t pid);
int shmem_cpuinfo__check_cpu_availability(pid_t pid, int cpu);
bool shmem_cpuinfo__exists(void);
void shmem_cpuinfo__enable_request_queues(void);
void shmem_cpuinfo__remove_requests(pid_t pid);
int shmem_cpuinfo__version(void);
size_t shmem_cpuinfo__size(void);

void shmem_cpuinfo__print_info(const char *shmem_key, int shmem_color, int columns,
        dlb_printshmem_flags_t print_flags);

int shmem_cpuinfo_testing__get_num_proc_requests(void);
int shmem_cpuinfo_testing__get_num_cpu_requests(int cpuid);
const cpu_set_t* shmem_cpuinfo_testing__get_free_cpu_set(void);
const cpu_set_t* shmem_cpuinfo_testing__get_occupied_core_set(void);
#endif /* SHMEM_CPUINFO_H */
