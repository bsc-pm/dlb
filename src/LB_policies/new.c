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

#include "LB_policies/new.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>

static int node_size;

int new_Init(const subprocess_descriptor_t *spd) {
    node_size = mu_get_system_size();
    return DLB_SUCCESS;
}

int new_Finish(const subprocess_descriptor_t *spd) {
    int error = new_Reclaim(spd);
    return (error >= 0) ? DLB_SUCCESS : error;
}

int new_DisableDLB(const subprocess_descriptor_t *spd) {
    shmem_cpuinfo__reset(spd->id);
    set_mask(&spd->pm, &spd->process_mask);
    return DLB_SUCCESS;
}


/* Keep active_mask updated?
 *  Update it requires:
 *      spd->lock
 *      spd passed as non-const
 *      info replicated
 *  If it wasn't updated:
 *      spd lock not needed?
 *      IntoBlocking and OutOfBlocking should trust the mask to the shmem
 *
 */

int new_IntoBlockingCall(const subprocess_descriptor_t *spd) {
    int error = DLB_NOUPDT;
    if (spd->options.mpi_lend_mode == BLOCK) {
        error = new_LendCpu(spd, sched_getcpu());
    }
    return error;
}

int new_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {
    int error = DLB_NOUPDT;
    if (spd->options.mpi_lend_mode == BLOCK) {
        error = new_AcquireCpu(spd, sched_getcpu());
    }
    return error;
}


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

int new_Lend(const subprocess_descriptor_t *spd) {
    // what mask to lend?
    // implement shmem_lend_all_except_one() ?
    return 0;
}

int new_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {
    /* lock spd? */
    pid_t new_pid = 0;
    bool async = spd->options.mode == MODE_ASYNC;
    int error = shmem_cpuinfo__add_cpu(spd->id, cpuid, async ? &new_pid : NULL);
    if (error == DLB_SUCCESS) {
        //CPU_CLR(cpuid, &spd->active_mask);
        if (async && new_pid) {
            shmem_async_enable_cpu(new_pid, cpuid);
        }
    }
    return error;
}

int new_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *new_pids = async ? calloc(node_size, sizeof(pid_t)) : NULL;
    int error = shmem_cpuinfo__add_cpu_mask(spd->id, mask, new_pids);
    if (error == DLB_SUCCESS) {
        //mu_substract(&spd->active_mask, &spd->active_mask, mask);
        if (async) {
            int cpuid;
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                pid_t new_pid = new_pids[cpuid];
                if (new_pid == spd->id) {
                    shmem_async_enable_cpu(new_pid, cpuid);
                }
            }
        }
    }
    return error;
}


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

int new_Reclaim(const subprocess_descriptor_t *spd) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_all(spd->id, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (async) {
                if (victim == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else if (victim != 0) {
                    shmem_async_disable_cpu(victim, cpuid);
                }
            } else {
                if (victim == spd->id) {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    free(victimlist);
    return error;
}

int new_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t victim = 0;
    bool async = spd->options.mode == MODE_ASYNC;
    int error = shmem_cpuinfo__recover_cpu(spd->id, cpuid, &victim);
    if (error == DLB_SUCCESS) {
        if (async) {
            shmem_async_enable_cpu(spd->id, cpuid);
        } else {
            enable_cpu(&spd->pm, cpuid);
        }
    }
    if (error == DLB_NOTED) {
        if (async) {
            shmem_async_disable_cpu(victim, cpuid);
        }
    }
    return error;
}

int new_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_cpus(spd->id, ncpus, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (async) {
                if (victim == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else if (victim != 0) {
                    shmem_async_disable_cpu(victim, cpuid);
                }
            } else {
                if (victim == spd->id) {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    free(victimlist);
    return error;
}

int new_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_cpu_mask(spd->id, mask, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (async) {
                if (victim == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else if (victim != 0) {
                    shmem_async_disable_cpu(victim, cpuid);
                }
            } else {
                if (victim == spd->id) {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    free(victimlist);
    return error;
}


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

int new_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t victim = 0;
    bool async = spd->options.mode == MODE_ASYNC;
    int error = shmem_cpuinfo__acquire_cpu(spd->id, cpuid, &victim);
    if (!async && error >= 0 ) {
        // In polling mode, enable CPU even if that causes oversubscription (DLB_NOTED err)
        // The acquired CPU **may** prevent CPU sharing by polling checkCPUAvailability,
        // but it's entirely decided by the caller runtime
        enable_cpu(&spd->pm, cpuid);
    } else if (async) {
        if (error == DLB_SUCCESS) {
            shmem_async_enable_cpu(spd->id, cpuid);
        } else if (error == DLB_NOTED) {
            shmem_async_disable_cpu(victim, cpuid);
        }
    }
    return error;
}

int new_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error;
    if (spd->options.mode == MODE_POLLING) {
        /* In polling mode, just borrow idle CPUs */
        error = new_BorrowCpus(spd, ncpus);
    } else {
        /* In async mode, only enable acquired idle CPUs, don't reclaim any */
        pid_t *victimlist = calloc(node_size, sizeof(pid_t));
        error = shmem_cpuinfo__acquire_cpus(spd->id, spd->options.priority,
                spd->cpus_priority_array, ncpus, victimlist);
        if (error == DLB_SUCCESS || error == DLB_NOTED) {
            int cpuid;
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                pid_t victim = victimlist[cpuid];
                if (victim == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                }
            }
        }
        free(victimlist);
    }
    return error;
}

int new_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__acquire_cpu_mask(spd->id, mask, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (async) {
                if (victim == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else if (victim != 0) {
                    shmem_async_disable_cpu(victim, cpuid);
                }
            } else {
                if (victim == spd->id) {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    free(victimlist);
    return error;
}


/*********************************************************************************/
/*    Borrow                                                                     */
/*********************************************************************************/

int new_Borrow(const subprocess_descriptor_t *spd) {
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__borrow_all(spd->id, spd->options.priority,
            spd->cpus_priority_array, victimlist);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (victim == spd->id) {
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    free(victimlist);
    return error;
}

int new_BorrowCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t victim = 0;
    int error = shmem_cpuinfo__borrow_cpu(spd->id, cpuid, &victim);
    // ignore victim? if error == DLB_SUCCESS victim should always be == pid
    if (error == DLB_SUCCESS) {
        enable_cpu(&spd->pm, cpuid);
    }
    return error;
}

int new_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__borrow_cpus(spd->id, spd->options.priority,
            spd->cpus_priority_array, ncpus, victimlist);
    /* FIXME: success only if ncpus == 0 ??? */
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (victim == spd->id) {
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    free(victimlist);
    return error;
}

int new_BorrowCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t *victimlist = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__borrow_cpu_mask(spd->id, mask, victimlist);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (victim == spd->id) {
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    free(victimlist);
    return error;
}


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

int new_Return(const subprocess_descriptor_t *spd) {
    if (spd->options.mode == MODE_ASYNC) {
        // ReturnAll should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    pid_t *new_pids = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__return_all(spd->id, new_pids);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (new_pids[cpuid] != 0) {
                disable_cpu(&spd->pm, cpuid);
            }
        }
    }
    free(new_pids);
    return error;
}

int new_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_pid = 0;
    bool async = spd->options.mode == MODE_ASYNC;
    int error = shmem_cpuinfo__return_cpu(spd->id, cpuid, &new_pid);
    if (error == DLB_SUCCESS) {
        //CPU_CLR(cpuid, &spd->active_mask);
        if (async) {
            shmem_async_enable_cpu(new_pid, cpuid);
        } else {
            disable_cpu(&spd->pm, cpuid);
        }
    }
    return error;
}

int new_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *new_pids = calloc(node_size, sizeof(pid_t));
    int error = shmem_cpuinfo__return_cpu_mask(spd->id, mask, new_pids);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_pid = new_pids[cpuid];
            if (new_pid != 0) {
                if (async) {
                    shmem_async_enable_cpu(new_pid, cpuid);
                } else {
                    disable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}


// Others

int new_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid) {
    return shmem_cpuinfo__is_cpu_available(spd->id, cpuid);
}

int new_UpdatePriorityCpus(subprocess_descriptor_t *spd, const cpu_set_t *process_mask) {
    int *prio1 = malloc(node_size*sizeof(int));
    int *prio2 = malloc(node_size*sizeof(int));
    int *prio3 = malloc(node_size*sizeof(int));
    int i, i1 = 0, i2 = 0, i3 = 0;

    priority_t priority = spd->options.priority;
    cpu_set_t affinity_mask;
    mu_get_parents_covering_cpuset(&affinity_mask, process_mask);
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        if (CPU_ISSET(cpuid, process_mask)) {
            prio1[i1++] = cpuid;
        } else {
            switch (priority) {
                case PRIO_NONE:
                    prio2[i2++] = cpuid;
                    break;
                case PRIO_AFFINITY_FIRST:
                    if (CPU_ISSET(cpuid, &affinity_mask)) {
                        prio2[i2++] = cpuid;
                    } else {
                        prio3[i3++] = cpuid;
                    }
                    break;
                case PRIO_AFFINITY_ONLY:
                    if (CPU_ISSET(cpuid, &affinity_mask)) {
                        prio2[i2++] = cpuid;
                    }
                    break;
                case PRIO_AFFINITY_FULL:
                    // This case cannot be pre-computed
                    break;
            }
        }
    }

    /* Merge [<[prio1][prio2][prio3][-1]>] */
    memmove(&spd->cpus_priority_array[0], prio1, sizeof(int)*i1);
    memmove(&spd->cpus_priority_array[i1], prio2, sizeof(int)*i2);
    memmove(&spd->cpus_priority_array[i1+i2], prio3, sizeof(int)*i3);
    for (i=i1+i2+i3; i<node_size; ++i) spd->cpus_priority_array[i] = -1;

    free(prio1);
    free(prio2);
    free(prio3);

    return 0;
}
