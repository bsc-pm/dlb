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

#include <sched.h>
#include <stdlib.h>

int new_Init(const subprocess_descriptor_t *spd) {
    return DLB_SUCCESS;
}

int new_Finish(const subprocess_descriptor_t *spd) {
    int error = new_Reclaim(spd);
    return (error >= 0) ? DLB_SUCCESS : error;
}

int new_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {
    /* lock spd? */
    int error;
    /* active_mask is not updated with helper threads, unless we decide to do it... */
    if (false /*!CPU_ISSET(cpuid, &spd->active_mask)*/) {
        // How to deal with unwanted acquires then?
        error = DLB_ERR_PERM;
    } else {
        pid_t new_pid = 0;
        error = shmem_cpuinfo__add_cpu(spd->id, cpuid, &new_pid);
        if (!error) {
            CPU_CLR(cpuid, &spd->active_mask);
            if (spd->options.mode == MODE_ASYNC && new_pid) {
                shmem_async_enable_cpu(new_pid, cpuid);
            }
        }
    }
    return error;
}

int new_Reclaim(const subprocess_descriptor_t *spd) {
    int nelems = mu_get_system_size();
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_all(spd->id, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
            pid_t victim = victimlist[cpuid];
            if (victim == spd->id) {
                shmem_async_enable_cpu(spd->id, cpuid);
            } else if (victim != 0) {
                shmem_async_disable_cpu(victim, cpuid);
            }
        }
    }
    free(victimlist);
    return error;
}

int new_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    pid_t victim = 0;
    error = shmem_cpuinfo__recover_cpu(spd->id, cpuid, &victim);
    if (error == DLB_SUCCESS) {
        shmem_async_enable_cpu(spd->id, cpuid);
    }
    if (error == DLB_NOTED && victim) {
        shmem_async_disable_cpu(victim, cpuid);
    }
    return error;
}

int new_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    pid_t victim = 0;
    error = shmem_cpuinfo__collect_cpu(spd->id, cpuid, &victim);
    if (error == DLB_SUCCESS) {
        shmem_async_enable_cpu(spd->id, cpuid);
    }
    if (error == DLB_NOTED && victim) {
        shmem_async_disable_cpu(victim, cpuid);
    }
    return error;
}

int new_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t new_pid = 0;
    int error = shmem_cpuinfo__return_cpu(spd->id, cpuid, &new_pid);
    if (error == DLB_NOTED) {
        CPU_CLR(cpuid, &spd->active_mask);
        if (spd->options.mode == MODE_ASYNC && new_pid) {
            shmem_async_enable_cpu(new_pid, cpuid);
        }
    }
    return error;
}
