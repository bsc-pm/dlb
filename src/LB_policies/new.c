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

int new_Init(const subprocess_descriptor_t *spd) {
    return DLB_SUCCESS;
}

int new_Finish(const subprocess_descriptor_t *spd) {
    int error = new_Reclaim(spd);
    return (error >= 0) ? DLB_SUCCESS : error;
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
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *new_pids = async ? calloc(nelems, sizeof(pid_t)) : NULL;
    int error = shmem_cpuinfo__add_cpu_mask(spd->id, mask, new_pids);
    if (error == DLB_SUCCESS) {
        //mu_substract(&spd->active_mask, &spd->active_mask, mask);
        if (async) {
            int cpuid;
            for (cpuid=0; cpuid<nelems; ++cpuid) {
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
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_all(spd->id, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_cpus(spd->id, ncpus, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__recover_cpu_mask(spd->id, mask, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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

int new_Acquire(const subprocess_descriptor_t *spd) {
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__collect_all(spd->id, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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

int new_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    pid_t victim = 0;
    bool async = spd->options.mode == MODE_ASYNC;
    int error = shmem_cpuinfo__collect_cpu(spd->id, cpuid, &victim);
    // FIXME: in polling mode, we may return DLB_NOUPDT here becase of the guest field
    if (!async && error == DLB_NOUPDT) error = DLB_SUCCESS;
    // EOFixme
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

int new_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__collect_cpus(spd->id, ncpus, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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

int new_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int nelems = mu_get_system_size();
    bool async = spd->options.mode == MODE_ASYNC;
    pid_t *victimlist = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__collect_cpu_mask(spd->id, mask, victimlist);
    if (error == DLB_SUCCESS || error == DLB_NOTED) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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
/*    Return                                                                     */
/*********************************************************************************/

int new_Return(const subprocess_descriptor_t *spd) {
    if (spd->options.mode == MODE_ASYNC) {
        // ReturnAll should not be called in async mode
        return DLB_ERR_NOCOMP;
    }

    int nelems = mu_get_system_size();
    pid_t *new_pids = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__return_all(spd->id, new_pids);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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
    int nelems = mu_get_system_size();
    pid_t *new_pids = calloc(nelems, sizeof(pid_t));
    int error = shmem_cpuinfo__return_cpu_mask(spd->id, mask, new_pids);
    if (error == DLB_SUCCESS) {
        int cpuid;
        for (cpuid=0; cpuid<nelems; ++cpuid) {
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
