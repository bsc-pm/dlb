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

#include "LB_policies/lewi_mask.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>

/* Node size is constant across subprocesses, it is safe to be shared */
static int node_size = -1;

/* These variables cannot be shared, so they need a private allocation */
typedef struct LeWI_mask_info {
    int64_t last_borrow;
    int *cpus_priority_array;
} lewi_info_t;


int lewi_mask_Init(subprocess_descriptor_t *spd) {
    /* Value is always updated to allow testing different node sizes */
    node_size = mu_get_system_size();

    /* Allocate and initialize private structure */
    spd->lewi_info = malloc(sizeof(lewi_info_t));
    lewi_info_t *lewi_info = spd->lewi_info;
    lewi_info->last_borrow = 0;
    lewi_info->cpus_priority_array = malloc(node_size*sizeof(int));
    lewi_mask_UpdateOwnershipInfo(spd, &spd->process_mask);

    /* Enable request queues only in async mode */
    if (spd->options.mode == MODE_ASYNC) {
        shmem_cpuinfo__enable_request_queues();
    }

    return DLB_SUCCESS;
}

int lewi_mask_Finalize(subprocess_descriptor_t *spd) {
    /* Reclaim all CPUs just before de-registering process */
    int error = lewi_mask_Reclaim(spd);

    /* De-allocate private structure */
    free(((lewi_info_t*)spd->lewi_info)->cpus_priority_array);
    free(spd->lewi_info);
    spd->lewi_info = NULL;

    return (error >= 0) ? DLB_SUCCESS : error;
}

int lewi_mask_EnableDLB(const subprocess_descriptor_t *spd) {
    return DLB_SUCCESS;
}

int lewi_mask_DisableDLB(const subprocess_descriptor_t *spd) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    int error = shmem_cpuinfo__reset(spd->id, new_guests, victims);
    if (error == DLB_SUCCESS) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else if (new_guest >= 0) {
                    shmem_async_disable_cpu(spd->id, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    enable_cpu(&spd->pm, cpuid);
                } else if (new_guest >= 0) {
                    disable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd) {
    int error = DLB_NOUPDT;
    if (spd->options.lewi_mpi) {
        error = lewi_mask_LendCpu(spd, sched_getcpu());
    }
    return error;
}

int lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {
    int error = DLB_NOUPDT;
    if (spd->options.lewi_mpi) {
        error = lewi_mask_AcquireCpu(spd, sched_getcpu());
    }
    return error;
}


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

int lewi_mask_Lend(const subprocess_descriptor_t *spd) {
    /* Lend all the CPUs except the current one */
    cpu_set_t mask;
    mu_get_system_mask(&mask);
    CPU_CLR(sched_getcpu(), &mask);
    return lewi_mask_LendCpuMask(spd, &mask);
}

int lewi_mask_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_guest;
    int error = shmem_cpuinfo__lend_cpu(spd->id, cpuid, &new_guest);
    if (error == DLB_SUCCESS) {
        if (spd->options.mode == MODE_ASYNC && new_guest > 0) {
            shmem_async_enable_cpu(new_guest, cpuid);
        }
    }
    return error;
}

int lewi_mask_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t new_guests[node_size];
    int error = shmem_cpuinfo__lend_cpu_mask(spd->id, mask, new_guests);
    if (error == DLB_SUCCESS) {
        if (spd->options.mode == MODE_ASYNC) {
            int cpuid;
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                pid_t new_guest = new_guests[cpuid];
                if (new_guest > 0) {
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            }
        }
    }
    return error;
}


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

int lewi_mask_Reclaim(const subprocess_descriptor_t *spd) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    int error = shmem_cpuinfo__reclaim_all(spd->id, new_guests, victims);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    /* If the CPU is guested, just disable visitor */
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    /* Only enable if the CPU is free */
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    /* Oversubscribe even if the CPU is guested */
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_guest;
    pid_t victim;
    int error = shmem_cpuinfo__reclaim_cpu(spd->id, cpuid, &new_guest, &victim);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        if (spd->options.mode == MODE_ASYNC) {
            if (victim > 0) {
                /* If the CPU is guested, just disable visitor */
                shmem_async_disable_cpu(victim, cpuid);
            } else if (new_guest == spd->id) {
                /* Only enable if the CPU is free */
                shmem_async_enable_cpu(new_guest, cpuid);
            }
        } else {
            if (new_guest == spd->id) {
                /* Oversubscribe even if the CPU is guested */
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    return error;
}

int lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    int error = shmem_cpuinfo__reclaim_cpus(spd->id, ncpus, new_guests, victims);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    /* If the CPU is guested, just disable visitor */
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    /* Only enable if the CPU is free */
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    /* Oversubscribe even if the CPU is guested */
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    int error = shmem_cpuinfo__reclaim_cpu_mask(spd->id, mask, new_guests, victims);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    /* If the CPU is guested, just disable visitor */
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    /* Only enable if the CPU is free */
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    /* Oversubscribe even if the CPU is guested */
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

int lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_guest;
    pid_t victim;
    int error = shmem_cpuinfo__acquire_cpu(spd->id, cpuid, &new_guest, &victim);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        if (spd->options.mode == MODE_ASYNC) {
            if (victim > 0) {
                /* If the CPU is guested, just disable visitor */
                shmem_async_disable_cpu(victim, cpuid);
            } else if (new_guest == spd->id) {
                /* Only enable if the CPU is free */
                shmem_async_enable_cpu(new_guest, cpuid);
            }
        } else {
            if (new_guest == spd->id) {
                /* Oversubscribe even if the CPU is guested */
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    return error;
}

int lewi_mask_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    bool async = spd->options.mode == MODE_ASYNC;
    int64_t *last_borrow = async ? NULL : &((lewi_info_t*)spd->lewi_info)->last_borrow;
    int *cpus_priority_array = ((lewi_info_t*)spd->lewi_info)->cpus_priority_array;
    int error = shmem_cpuinfo__acquire_cpus(spd->id, spd->options.lewi_affinity,
            cpus_priority_array, last_borrow, ncpus, new_guests, victims);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    /* If the CPU is guested, just disable visitor */
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    /* Only enable if the CPU is free */
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    /* Oversubscribe even if the CPU is guested */
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t new_guests[node_size];
    pid_t victims[node_size];
    int error = shmem_cpuinfo__acquire_cpu_mask(spd->id, mask, new_guests, victims);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            pid_t victim = victims[cpuid];
            if (async) {
                if (victim > 0) {
                    /* If the CPU is guested, just disable visitor */
                    shmem_async_disable_cpu(victim, cpuid);
                } else if (new_guest == spd->id) {
                    /* Only enable if the CPU is free */
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest == spd->id) {
                    /* Oversubscribe even if the CPU is guested */
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}


/*********************************************************************************/
/*    Borrow                                                                     */
/*********************************************************************************/

int lewi_mask_Borrow(const subprocess_descriptor_t *spd) {
    pid_t new_guests[node_size];
    bool async = spd->options.mode == MODE_ASYNC;
    int64_t *last_borrow = async ? NULL : &((lewi_info_t*)spd->lewi_info)->last_borrow;
    int *cpus_priority_array = ((lewi_info_t*)spd->lewi_info)->cpus_priority_array;
    int error = shmem_cpuinfo__borrow_all(spd->id, spd->options.lewi_affinity,
            cpus_priority_array, last_borrow, new_guests);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (new_guests[cpuid] == spd->id) {
                if (async) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_BorrowCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_guest;
    int error = shmem_cpuinfo__borrow_cpu(spd->id, cpuid, &new_guest);
    if (error == DLB_SUCCESS) {
        if (new_guest == spd->id) {
            if (spd->options.mode == MODE_ASYNC) {
                shmem_async_enable_cpu(spd->id, cpuid);
            } else {
                enable_cpu(&spd->pm, cpuid);
            }
        }
    }
    return error;
}

int lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    pid_t new_guests[node_size];
    bool async = spd->options.mode == MODE_ASYNC;
    int64_t *last_borrow = async ? NULL : &((lewi_info_t*)spd->lewi_info)->last_borrow;
    int *cpus_priority_array = ((lewi_info_t*)spd->lewi_info)->cpus_priority_array;
    int error = shmem_cpuinfo__borrow_cpus(spd->id, spd->options.lewi_affinity,
            cpus_priority_array, last_borrow, ncpus, new_guests);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (new_guests[cpuid] == spd->id) {
                if (async) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}

int lewi_mask_BorrowCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t new_guests[node_size];
    int error = shmem_cpuinfo__borrow_cpu_mask(spd->id, mask, new_guests);
    if (error == DLB_SUCCESS) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (new_guests[cpuid] == spd->id) {
                if (async) {
                    shmem_async_enable_cpu(spd->id, cpuid);
                } else {
                    enable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

int lewi_mask_Return(const subprocess_descriptor_t *spd) {
    if (spd->options.mode == MODE_ASYNC) {
        // ReturnAll should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    pid_t new_guests[node_size];
    int error = shmem_cpuinfo__return_all(spd->id, new_guests);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            if (new_guest >= 0 && new_guest != spd->id) {
                disable_cpu(&spd->pm, cpuid);
            }
        }
    }
    return error;
}

int lewi_mask_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_guest;
    int error = shmem_cpuinfo__return_cpu(spd->id, cpuid, &new_guest);
    if (error == DLB_SUCCESS) {
        if (spd->options.mode == MODE_ASYNC) {
            if (new_guest > 0) {
                shmem_async_enable_cpu(new_guest, cpuid);
            }
        } else {
            if (new_guest >= 0 && new_guest != spd->id) {
                disable_cpu(&spd->pm, cpuid);
            }
        }
    }
    return error;
}

int lewi_mask_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    pid_t new_guests[node_size];
    int error = shmem_cpuinfo__return_cpu_mask(spd->id, mask, new_guests);
    if (error == DLB_SUCCESS) {
        bool async = spd->options.mode == MODE_ASYNC;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            pid_t new_guest = new_guests[cpuid];
            if (async) {
                if (new_guest > 0) {
                    shmem_async_enable_cpu(new_guest, cpuid);
                }
            } else {
                if (new_guest >= 0 && new_guest != spd->id) {
                    disable_cpu(&spd->pm, cpuid);
                }
            }
        }
    }
    return error;
}


// Others

int lewi_mask_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid) {
    return shmem_cpuinfo__check_cpu_availability(spd->id, cpuid);
}

/* Construct a priority list of CPUs merging 4 lists:
 *  prio1: always owned CPUs
 *  prio2: nearby CPUs depending on the affinity option
 *  prio3: other CPUs in some affinity options
 *  rest:  fill '-1' to indicate non eligible CPUs
 */
int lewi_mask_UpdateOwnershipInfo(const subprocess_descriptor_t *spd,
        const cpu_set_t *process_mask) {
    int *prio1 = malloc(node_size*sizeof(int));
    int *prio2 = malloc(node_size*sizeof(int));
    int *prio3 = malloc(node_size*sizeof(int));
    int i, i1 = 0, i2 = 0, i3 = 0;

    priority_t priority = spd->options.lewi_affinity;
    cpu_set_t affinity_mask;
    mu_get_parents_covering_cpuset(&affinity_mask, process_mask);
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        if (CPU_ISSET(cpuid, process_mask)) {
            prio1[i1++] = cpuid;
        } else {
            switch (priority) {
                case PRIO_ANY:
                    prio2[i2++] = cpuid;
                    break;
                case PRIO_NEARBY_FIRST:
                    if (CPU_ISSET(cpuid, &affinity_mask)) {
                        prio2[i2++] = cpuid;
                    } else {
                        prio3[i3++] = cpuid;
                    }
                    break;
                case PRIO_NEARBY_ONLY:
                    if (CPU_ISSET(cpuid, &affinity_mask)) {
                        prio2[i2++] = cpuid;
                    }
                    break;
                case PRIO_SPREAD_IFEMPTY:
                    // This case cannot be pre-computed
                    break;
            }
        }
    }

    /* Merge [<[prio1][prio2][prio3][-1]>] */
    int *cpus_priority_array = ((lewi_info_t*)spd->lewi_info)->cpus_priority_array;
    memmove(&cpus_priority_array[0], prio1, sizeof(int)*i1);
    memmove(&cpus_priority_array[i1], prio2, sizeof(int)*i2);
    memmove(&cpus_priority_array[i1+i2], prio3, sizeof(int)*i3);
    for (i=i1+i2+i3; i<node_size; ++i) cpus_priority_array[i] = -1;

    free(prio1);
    free(prio2);
    free(prio3);

    return 0;
}
