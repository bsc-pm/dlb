/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "LB_policies/lewi_mask.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/gslist.h"
#include "support/mask_utils.h"
#include "support/small_array.h"
#include "support/types.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"


/* Node size will be the same for all processes in the node,
 * it is safe to be out of the shared memory */
static int node_size = -1;

/* LeWI_mask data is private for each process */
typedef struct LeWI_mask_info {
    int64_t last_borrow;
    int max_parallelism;
    array_cpuid_t cpus_priority_array;
    cpu_set_t pending_reclaimed_cpus;       /* CPUs that become reclaimed after an MPI */
    cpu_set_t in_mpi_cpus;                  /* CPUs inside an MPI call */
    GSList *cpuid_arrays;                   /* thread-private pointers to free at finalize */
    GSList *cpuinfo_task_arrays;            /* thread-private pointers to free at finalize */
    pthread_mutex_t mutex;                  /* Mutex to protect lewi_info */
} lewi_info_t;


/* Compute the common elements between a cpuid array a cpu_set:
 * cpuid_t *result = cpuid_t *op1 AND cpu_set_t *op2
 * (cpu_set_t* may be NULL, meaning neutral value)
 */
static inline void cpu_array_and(array_cpuid_t *result,
        const array_cpuid_t *op1, const cpu_set_t *op2) {
    for (unsigned int i = 0; i < op1->count; ++i) {
        if (op2 == NULL || CPU_ISSET(op1->items[i], op2)) {
            array_cpuid_t_push(result, op1->items[i]);
        }
    }
}

/* Construct a cpuid array from a cpu_set_t */
static inline void cpu_array_from_cpuset(array_cpuid_t *result,
        const cpu_set_t *cpu_set) {
    for (int cpuid = mu_get_first_cpu(cpu_set);
            cpuid >= 0;
            cpuid = mu_get_next_cpu(cpu_set, cpuid)) {
        array_cpuid_t_push(result, cpuid);
    }
}


/* Construct a priority list of CPUs considering the topology and the affinity mask */
static void lewi_mask_UpdateOwnershipInfo(const subprocess_descriptor_t *spd,
        const cpu_set_t *process_mask) {

    lewi_info_t *lewi_info = spd->lewi_info;
    lewi_affinity_t lewi_affinity = spd->options.lewi_affinity;

    cpu_set_t affinity_mask;
    mu_get_nodes_intersecting_with_cpuset(&affinity_mask, process_mask);

    /* Reset current priority array */
    array_cpuid_t *cpus_priority_array = &lewi_info->cpus_priority_array;
    array_cpuid_t_clear(cpus_priority_array);

    /* First, construct a list of potentially available CPUs
     * (owned + nearby, depending on the affinity policy) */
    cpu_set_t system_mask;
    mu_get_system_mask(&system_mask);
    for (int cpuid = mu_get_first_cpu(&system_mask);
            cpuid >= 0;
            cpuid = mu_get_next_cpu(&system_mask, cpuid)) {
        if (CPU_ISSET(cpuid, process_mask)) {
            array_cpuid_t_push(cpus_priority_array, cpuid);
        } else {
            switch (lewi_affinity) {
                case LEWI_AFFINITY_AUTO:
                case LEWI_AFFINITY_MASK:
                case LEWI_AFFINITY_NEARBY_FIRST:
                    array_cpuid_t_push(cpus_priority_array, cpuid);
                    break;
                case LEWI_AFFINITY_NEARBY_ONLY:
                    if (CPU_ISSET(cpuid, &affinity_mask)) {
                        array_cpuid_t_push(cpus_priority_array, cpuid);
                    }
                    break;
                case LEWI_AFFINITY_SPREAD_IFEMPTY:
                    // This case cannot be pre-computed
                    break;
                case LEWI_AFFINITY_NONE:
                    /* LEWI_AFFINITY_NONE should force LeWI without mask support */
                    fatal("Unhandled LEWI_AFFINITY_NONE in LeWI mask. "
                            "Please report bug");
            }
        }
    }

    /* Sort available CPUs according to the affinity */
    cpu_set_t affinity[3];
    memcpy(&affinity[0], process_mask, sizeof(cpu_set_t));
    memcpy(&affinity[1], &affinity_mask, sizeof(cpu_set_t));
    CPU_ZERO(&affinity[2]);
    qsort_r(cpus_priority_array->items, cpus_priority_array->count,
            sizeof(cpuid_t), mu_cmp_cpuids_by_affinity, &affinity);
}


/*********************************************************************************/
/*    Thread-local pointers for temporary storage during LeWI calls              */
/*********************************************************************************/

/* Array of eligible CPUs. Usually a subset of the CPUs priority array. */
static __thread array_cpuid_t _cpu_subset = {};

static inline array_cpuid_t* get_cpu_subset(const subprocess_descriptor_t *spd) {
    /* Thread already has an allocated array, return it */
    if (likely(_cpu_subset.items != NULL)) {
        array_cpuid_t_clear(&_cpu_subset);
        return &_cpu_subset;
    }

    /* Otherwise, allocate */
    array_cpuid_t_init(&_cpu_subset, node_size);

    /* Add pointer to lewi_info to deallocate later */
    lewi_info_t *lewi_info = spd->lewi_info;
    pthread_mutex_lock(&lewi_info->mutex);
    {
        lewi_info->cpuid_arrays =
            g_slist_prepend(lewi_info->cpuid_arrays, &_cpu_subset);
    }
    pthread_mutex_unlock(&lewi_info->mutex);

    return &_cpu_subset;
}

/* Array of cpuinfo tasks. For enabling or disabling CPUs. */
static __thread array_cpuinfo_task_t _tasks = {};

static inline array_cpuinfo_task_t* get_tasks(const subprocess_descriptor_t *spd) {
    /* Thread already has an allocated array, return it */
    if (likely(_tasks.items != NULL)) {
        array_cpuinfo_task_t_clear(&_tasks);
        return &_tasks;
    }

    /* Otherwise, allocate */
    array_cpuinfo_task_t_init(&_tasks, node_size*2);

    /* Add pointer to lewi_info to deallocate later */
    lewi_info_t *lewi_info = spd->lewi_info;
    pthread_mutex_lock(&lewi_info->mutex);
    {
        lewi_info->cpuinfo_task_arrays =
            g_slist_prepend(lewi_info->cpuinfo_task_arrays, &_tasks);
    }
    pthread_mutex_unlock(&lewi_info->mutex);

    return &_tasks;
}


/*********************************************************************************/
/*    Resolve cpuinfo tasks                                                      */
/*********************************************************************************/

static void resolve_cpuinfo_tasks(const subprocess_descriptor_t *restrict spd,
        const array_cpuinfo_task_t *restrict tasks) {

    size_t tasks_count = tasks->count;

    /* We don't need it strictly sorted, but if there are 3 or more tasks
     * we need to group them by pid */
    if (tasks_count > 2 ) {
        array_cpuinfo_task_t_sort(&_tasks);
    }

    /* Iterate tasks by group of PIDs */
    for (size_t i=0; i<tasks_count; ) {

        /* count how many times this PID is repeated */
        size_t j = i+1;
        while (j < tasks_count
                && tasks->items[j].pid == tasks->items[i].pid) {
            ++j;
        }
        size_t num_tasks = j - i;

        if (num_tasks == 1) {
            /* resolve single task */
            const cpuinfo_task_t *task = &tasks->items[i];
            if (task->pid == spd->id) {
                if (task->action == ENABLE_CPU) {
                    enable_cpu(&spd->pm, task->cpuid);
                }
                else if (task->action == DISABLE_CPU) {
                    disable_cpu(&spd->pm, task->cpuid);
                }
            }
            else if (spd->options.mode == MODE_ASYNC) {
                if (task->action == ENABLE_CPU) {
                    shmem_async_enable_cpu(task->pid, task->cpuid);
                }
                else if (task->action == DISABLE_CPU) {
                    shmem_async_disable_cpu(task->pid, task->cpuid);
                    shmem_cpuinfo__return_async_cpu(task->pid, task->cpuid);
                }
            }
        } else {
            /* group tasks */
            cpu_set_t cpus_to_enable = {};
            cpu_set_t cpus_to_disable = {};
            for (size_t k = i; k < i+num_tasks; ++k) {
                const cpuinfo_task_t *task = &tasks->items[k];
                if (task->action == ENABLE_CPU) {
                    CPU_SET(task->cpuid, &cpus_to_enable);
                }
                else if (task->action == DISABLE_CPU) {
                    CPU_SET(task->cpuid, &cpus_to_disable);
                }
            }

            /* resolve group of tasks for the same PID */
            const cpuinfo_task_t *task = &tasks->items[i];
            if (task->pid == spd->id) {
                if (CPU_COUNT(&cpus_to_enable) > 0) {
                    enable_cpu_set(&spd->pm, &cpus_to_enable);
                }
                if (CPU_COUNT(&cpus_to_disable) > 0) {
                    disable_cpu_set(&spd->pm, &cpus_to_disable);
                }
            }
            else if (spd->options.mode == MODE_ASYNC) {
                if (CPU_COUNT(&cpus_to_enable) > 0) {
                    shmem_async_enable_cpu_set(task->pid, &cpus_to_enable);
                }
                if (CPU_COUNT(&cpus_to_disable) > 0) {
                    shmem_async_disable_cpu_set(task->pid, &cpus_to_disable);
                    shmem_cpuinfo__return_async_cpu_mask(task->pid, &cpus_to_disable);
                }
            }
        }

        i += num_tasks;
    }
}


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

int lewi_mask_Init(subprocess_descriptor_t *spd) {
    /* Value is always updated to allow testing different node sizes */
    node_size = mu_get_system_size();

    /* Allocate and initialize private structure */
    spd->lewi_info = malloc(sizeof(lewi_info_t));
    lewi_info_t *lewi_info = spd->lewi_info;
    *lewi_info = (const lewi_info_t) {
        .max_parallelism = spd->options.lewi_max_parallelism,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
    };
    array_cpuid_t_init(&lewi_info->cpus_priority_array, node_size);
    lewi_mask_UpdateOwnershipInfo(spd, &spd->process_mask);

    /* Enable request queues only in async mode */
    if (spd->options.mode == MODE_ASYNC) {
        shmem_cpuinfo__enable_request_queues();
    }

    return DLB_SUCCESS;
}

int lewi_mask_Finalize(subprocess_descriptor_t *spd) {
    /* De-register subprocess from the shared memory */
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__deregister(spd->id, tasks);
    if (error == DLB_SUCCESS) {
        resolve_cpuinfo_tasks(spd, tasks);
    }

    /* Deallocate thread private arrays */
    lewi_info_t *lewi_info = spd->lewi_info;
    pthread_mutex_lock(&lewi_info->mutex);
    {
        for (GSList *node = lewi_info->cpuid_arrays;
                node != NULL;
                node = node->next) {
            array_cpuid_t *array = node->data;
            array_cpuid_t_destroy(array);
        }
        g_slist_free(lewi_info->cpuid_arrays);

        for (GSList *node = lewi_info->cpuinfo_task_arrays;
                node != NULL;
                node = node->next) {
            array_cpuid_t *array = node->data;
            array_cpuid_t_destroy(array);
        }
        g_slist_free(lewi_info->cpuinfo_task_arrays);
    }
    pthread_mutex_unlock(&lewi_info->mutex);


    /* Deallocate private structure */
    array_cpuid_t_destroy(&lewi_info->cpus_priority_array);
    free(lewi_info);
    lewi_info = NULL;

    return (error >= 0) ? DLB_SUCCESS : error;
}


/*********************************************************************************/
/*    LeWI Modes (enable/disable, max_parallelism, ...)                          */
/*********************************************************************************/

int lewi_mask_EnableDLB(const subprocess_descriptor_t *spd) {
    /* Reset value of last_borrow */
    ((lewi_info_t*)spd->lewi_info)->last_borrow = 0;
    return DLB_SUCCESS;
}

int lewi_mask_DisableDLB(const subprocess_descriptor_t *spd) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__reset(spd->id, tasks);
    if (error == DLB_SUCCESS) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}

int lewi_mask_SetMaxParallelism(const subprocess_descriptor_t *spd, int max) {
    int error = DLB_SUCCESS;
    if (max > 0) {
        lewi_info_t *lewi_info = spd->lewi_info;
        lewi_info->max_parallelism = max;
        array_cpuinfo_task_t *tasks = get_tasks(spd);
        error = shmem_cpuinfo__update_max_parallelism(spd->id, max, tasks);
        if (error == DLB_SUCCESS) {
            resolve_cpuinfo_tasks(spd, tasks);
        }
    }
    return error;
}

int lewi_mask_UnsetMaxParallelism(const subprocess_descriptor_t *spd) {
    lewi_info_t *lewi_info = spd->lewi_info;
    lewi_info->max_parallelism = 0;
    return DLB_SUCCESS;
}


/*********************************************************************************/
/*    MPI                                                                        */
/*********************************************************************************/

/* Obtain thread mask and remove first core if keep_cpu_on_blocking_call */
static inline void get_mask_for_blocking_call(
        cpu_set_t *cpu_set, bool keep_cpu_on_blocking_call) {

    /* Obtain current thread's affinity mask */
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), cpu_set);

    if (keep_cpu_on_blocking_call) {
        /* Remove the first core */
        int first_cpuid = mu_get_first_cpu(cpu_set);
        const mu_cpuset_t *first_core = mu_get_core_mask(first_cpuid);
        mu_substract(cpu_set, cpu_set, first_core->set);
    }
}

/* Lend the CPUs of the thread encountering the blocking call */
int lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd) {
    int error = DLB_NOUPDT;

    /* Obtain affinity mask to lend */
    cpu_set_t cpu_set;
    get_mask_for_blocking_call(&cpu_set,
            spd->options.lewi_keep_cpu_on_blocking_call);

    if (mu_count(&cpu_set) > 0) {
#ifdef DEBUG_VERSION
        /* Add cpu_set to in_mpi_cpus */
        lewi_info_t *lewi_info = spd->lewi_info;
        fatal_cond(mu_intersects(&lewi_info->in_mpi_cpus, &cpu_set),
                    "Some CPU in %s already into blocking call", mu_to_str(&cpu_set));
        mu_or(&lewi_info->in_mpi_cpus, &lewi_info->in_mpi_cpus, &cpu_set);
#endif
        verbose(VB_MICROLB, "In blocking call, lending %s", mu_to_str(&cpu_set));

        /* Finally, lend mask */
        error = lewi_mask_LendCpuMask(spd, &cpu_set);
    }
    return error;
}

/* Reclaim the CPUs that were lent when encountering the blocking call.
 * The thread must have not change its affinity mask since then. */
int lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd) {
    int error = DLB_NOUPDT;

    /* Obtain affinity mask to reclaim */
    cpu_set_t cpu_set;
    get_mask_for_blocking_call(&cpu_set,
            spd->options.lewi_keep_cpu_on_blocking_call);

    if (mu_count(&cpu_set) > 0) {
        lewi_info_t *lewi_info = spd->lewi_info;
#ifdef DEBUG_VERSION
        /* Clear cpu_set from in_mpi_cpus */
        fatal_cond(!mu_is_subset(&lewi_info->in_mpi_cpus, &cpu_set),
                "Some CPU in %s is not into blocking call", mu_to_str(&cpu_set));
        mu_substract(&lewi_info->in_mpi_cpus, &lewi_info->in_mpi_cpus, &cpu_set);
#endif
        verbose(VB_MICROLB, "Out of blocking call, acquiring %s", mu_to_str(&cpu_set));

        /* If there are owned CPUs to reclaim: */
        cpu_set_t owned_cpus;
        mu_and(&owned_cpus, &cpu_set, &spd->process_mask);
        if (mu_count(&owned_cpus) > 0) {

            /* Construct a CPU array from owned_cpus */
            array_cpuid_t *cpu_subset = get_cpu_subset(spd);
            cpu_array_from_cpuset(cpu_subset, &owned_cpus);

            /* Acquire CPUs, but do not call any enable CPU callback */
            array_cpuinfo_task_t *tasks = get_tasks(spd);
            error = shmem_cpuinfo__acquire_from_cpu_subset(spd->id, cpu_subset, tasks);

            /* Once CPUs are acquired from the shmem, we don't need to call
             * any ENABLE callback because they weren't disabled in IntoBlockingCall.
             * Only if asynchronous mode, keep DISABLE tasks, otherwise remove all. */
            if (error == DLB_NOTED
                    && spd->options.mode == MODE_ASYNC) {
                for (size_t i = 0, i_write = 0; i < tasks->count; ++i) {
                    const cpuinfo_task_t *task = &tasks->items[i];
                    if (task->action == DISABLE_CPU) {
                        if (i != i_write) {
                            tasks->items[i_write] = tasks->items[i];
                        }
                        ++i_write;
                    }
                }
                /* Async: resolve only DISABLE tasks */
                resolve_cpuinfo_tasks(spd, tasks);
            } else {
                /* Polling: clear tasks */
                array_cpuinfo_task_t_clear(tasks);
            }
        }

        /* If there are ONLY non-owned CPUs to reclaim:
         * (if some owned CPU was already reclaimed, we forget about non-owned) */
        cpu_set_t non_owned_cpus;
        mu_substract(&non_owned_cpus, &cpu_set, &spd->process_mask);
        if (mu_count(&non_owned_cpus) > 0
                && mu_count(&owned_cpus) == 0) {

            /* Construct a CPU array from non_owned_cpus */
            array_cpuid_t *cpu_subset = get_cpu_subset(spd);
            cpu_array_from_cpuset(cpu_subset, &non_owned_cpus);

            /* Borrow CPUs, but also ignoring the enable callbacks */
            array_cpuinfo_task_t *tasks = get_tasks(spd);
            error = shmem_cpuinfo__borrow_from_cpu_subset(spd->id, cpu_subset, tasks);

            if (error != DLB_SUCCESS) {
                /* Annotate the CPU as pending to bypass the shared memory for the next query */
                mu_or(&lewi_info->pending_reclaimed_cpus, &lewi_info->pending_reclaimed_cpus,
                        &non_owned_cpus);

                /* We lost the CPU while in MPI.
                    * In async mode, we can just disable the CPU.
                    * In polling mode, the process needs to call Return */
                if (spd->options.mode == MODE_ASYNC) {
                    disable_cpu_set(&spd->pm, &non_owned_cpus);
                }
            }
        }
    }

    return error;
}


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

int lewi_mask_Lend(const subprocess_descriptor_t *spd) {
    cpu_set_t mask;
    mu_get_system_mask(&mask);
    return lewi_mask_LendCpuMask(spd, &mask);
}

int lewi_mask_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__lend_cpu(spd->id, cpuid, tasks);

    if (error == DLB_SUCCESS) {
        if (spd->options.mode == MODE_ASYNC) {
            resolve_cpuinfo_tasks(spd, tasks);
        }

        /* Clear possible pending reclaimed CPUs */
        lewi_info_t *lewi_info = spd->lewi_info;
        CPU_CLR(cpuid, &lewi_info->pending_reclaimed_cpus);
    }
    return error;
}

int lewi_mask_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__lend_cpu_mask(spd->id, mask, tasks);
    if (error == DLB_SUCCESS) {
        if (spd->options.mode == MODE_ASYNC) {
            resolve_cpuinfo_tasks(spd, tasks);
        }

        /* Clear possible pending reclaimed CPUs */
        lewi_info_t *lewi_info = spd->lewi_info;
        mu_substract(&lewi_info->pending_reclaimed_cpus,
                &lewi_info->pending_reclaimed_cpus, mask);
    }
    return error;
}


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

int lewi_mask_Reclaim(const subprocess_descriptor_t *spd) {
    /* Even though shmem_cpuinfo__reclaim_all exists,
     * iterating the mask should be more efficient */
    return lewi_mask_ReclaimCpuMask(spd, &spd->process_mask);
}

int lewi_mask_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__reclaim_cpu(spd->id, cpuid, tasks);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}

int lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__reclaim_cpus(spd->id, ncpus, tasks);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}

int lewi_mask_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__reclaim_cpu_mask(spd->id, mask, tasks);
    resolve_cpuinfo_tasks(spd, tasks);
    return error;
}


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

int lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__acquire_cpu(spd->id, cpuid, tasks);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}

int lewi_mask_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_NOUPDT;
    if (ncpus == 0) {
        /* AcquireCPUs(0) has a special meaning of removing any previous request */
        shmem_cpuinfo__remove_requests(spd->id);
        error = DLB_SUCCESS;
    } else if (ncpus > 0) {
        error = lewi_mask_AcquireCpusInMask(spd, ncpus, NULL);
    }
    return error;
}

int lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return lewi_mask_AcquireCpusInMask(spd, 0, mask);
}

int lewi_mask_AcquireCpusInMask(const subprocess_descriptor_t *spd, int ncpus,
        const cpu_set_t *mask) {
    lewi_info_t *lewi_info = spd->lewi_info;
    bool async = spd->options.mode == MODE_ASYNC;
    int64_t *last_borrow = async ? NULL : &lewi_info->last_borrow;

    /* Construct a CPU array based on cpus_priority_array and mask (if present) */
    array_cpuid_t *cpu_subset = get_cpu_subset(spd);
    cpu_array_and(cpu_subset, &lewi_info->cpus_priority_array, mask);

    /* Provide a number of requested CPUs only if needed */
    int *requested_ncpus = ncpus > 0 ? &ncpus : NULL;

    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(spd->id,
            requested_ncpus, cpu_subset, spd->options.lewi_affinity,
            lewi_info->max_parallelism, last_borrow, tasks);

    if (error != DLB_NOUPDT) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}


/*********************************************************************************/
/*    Borrow                                                                     */
/*********************************************************************************/

int lewi_mask_Borrow(const subprocess_descriptor_t *spd) {
    cpu_set_t system_mask;
    mu_get_system_mask(&system_mask);
    return lewi_mask_BorrowCpusInMask(spd, 0, &system_mask);
}

int lewi_mask_BorrowCpu(const subprocess_descriptor_t *spd, int cpuid) {
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__borrow_cpu(spd->id, cpuid, tasks);
    if (error == DLB_SUCCESS) {
        resolve_cpuinfo_tasks(spd, tasks);
    }
    return error;
}

int lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    return lewi_mask_BorrowCpusInMask(spd, ncpus, NULL);
}

int lewi_mask_BorrowCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return lewi_mask_BorrowCpusInMask(spd, 0, mask);
}

int lewi_mask_BorrowCpusInMask(const subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask) {
    lewi_info_t *lewi_info = spd->lewi_info;
    bool async = spd->options.mode == MODE_ASYNC;
    int64_t *last_borrow = async ? NULL : &lewi_info->last_borrow;

    /* Construct a CPU array based on cpus_priority_array and mask (if present) */
    array_cpuid_t *cpu_subset = get_cpu_subset(spd);
    cpu_array_and(cpu_subset, &lewi_info->cpus_priority_array, mask);

    /* Provide a number of requested CPUs only if needed */
    int *requested_ncpus = ncpus > 0 ? &ncpus : NULL;

    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__borrow_ncpus_from_cpu_subset(spd->id,
            requested_ncpus, cpu_subset, spd->options.lewi_affinity,
            lewi_info->max_parallelism, last_borrow, tasks);

    if (error == DLB_SUCCESS) {
        resolve_cpuinfo_tasks(spd, tasks);
    }

    return error;
}


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

int lewi_mask_Return(const subprocess_descriptor_t *spd) {
    if (spd->options.mode == MODE_ASYNC) {
        // Return should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__return_all(spd->id, tasks);
    resolve_cpuinfo_tasks(spd, tasks);

    /* Check possible pending reclaimed CPUs */
    lewi_info_t *lewi_info = spd->lewi_info;
    int cpuid = mu_get_first_cpu(&lewi_info->pending_reclaimed_cpus);
    if (cpuid >= 0) {
        do {
            disable_cpu(&spd->pm, cpuid);
            cpuid = mu_get_next_cpu(&lewi_info->pending_reclaimed_cpus, cpuid);
        } while (cpuid >= 0);
        CPU_ZERO(&lewi_info->pending_reclaimed_cpus);
        error = DLB_SUCCESS;
    }

    return error;
}

int lewi_mask_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {
    if (spd->options.mode == MODE_ASYNC) {
        // Return should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__return_cpu(spd->id, cpuid, tasks);
    if (error == DLB_SUCCESS || error == DLB_ERR_REQST) {
        resolve_cpuinfo_tasks(spd, tasks);
    } else if (error == DLB_ERR_PERM) {
        /* Check possible pending reclaimed CPUs */
        lewi_info_t *lewi_info = spd->lewi_info;
        if (CPU_ISSET(cpuid, &lewi_info->pending_reclaimed_cpus)) {
            CPU_CLR(cpuid, &lewi_info->pending_reclaimed_cpus);
            disable_cpu(&spd->pm, cpuid);
            error = DLB_SUCCESS;
        }
    }
    return error;
}

int lewi_mask_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    if (spd->options.mode == MODE_ASYNC) {
        // Return should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    array_cpuinfo_task_t *tasks = get_tasks(spd);
    int error = shmem_cpuinfo__return_cpu_mask(spd->id, mask, tasks);
    resolve_cpuinfo_tasks(spd, tasks);

    /* Check possible pending reclaimed CPUs */
    lewi_info_t *lewi_info = spd->lewi_info;
    if (CPU_COUNT(&lewi_info->pending_reclaimed_cpus) > 0) {
        cpu_set_t cpus_to_return;
        CPU_AND(&cpus_to_return, &lewi_info->pending_reclaimed_cpus, mask);
        for (int cpuid = mu_get_first_cpu(&cpus_to_return); cpuid >= 0;
                cpuid = mu_get_next_cpu(&cpus_to_return, cpuid)) {
            disable_cpu(&spd->pm, cpuid);
        }
        mu_substract(&lewi_info->pending_reclaimed_cpus,
                &lewi_info->pending_reclaimed_cpus, &cpus_to_return);
        error = DLB_SUCCESS;
    }

    return error;
}


// Others

int lewi_mask_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid) {
    return shmem_cpuinfo__check_cpu_availability(spd->id, cpuid);
}

int lewi_mask_UpdateOwnership(const subprocess_descriptor_t *spd,
        const cpu_set_t *process_mask) {
    /* Update priority array */
    lewi_mask_UpdateOwnershipInfo(spd, process_mask);

    /* Update cpuinfo data and return reclaimed CPUs */
    array_cpuinfo_task_t *tasks = get_tasks(spd);
    shmem_cpuinfo__update_ownership(spd->id, process_mask, tasks);
    resolve_cpuinfo_tasks(spd, tasks);

    /* Check possible pending reclaimed CPUs */
    lewi_info_t *lewi_info = spd->lewi_info;
    int cpuid = mu_get_first_cpu(&lewi_info->pending_reclaimed_cpus);
    if (cpuid >= 0) {
        do {
            disable_cpu(&spd->pm, cpuid);
            cpuid = mu_get_next_cpu(&lewi_info->pending_reclaimed_cpus, cpuid);
        } while (cpuid >= 0);
        CPU_ZERO(&lewi_info->pending_reclaimed_cpus);
    }

    return DLB_SUCCESS;
}
