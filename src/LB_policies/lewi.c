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


#include "LB_policies/lewi.h"

#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "LB_comm/comm_lend_light.h"
#include "LB_numThreads/numThreads.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/types.h"

#include <sched.h>
#include <limits.h>


static int default_cpus;
static int myCPUS = 0;
static int enabled = 0;
static int single = 0;
static int max_parallelism = 0;
static void setThreads_Lend_light(const pm_interface_t *pm, int numThreads);

/******* Main Functions LeWI Balancing Policy ********/

int lewi_Init(subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "LeWI Init");

    default_cpus = CPU_COUNT(&spd->process_mask);

    setThreads_Lend_light(&spd->pm, default_cpus);

    info0("Default cpus per process: %d", default_cpus);

    bool greedy = spd->options.lewi_greedy;
    if (greedy) {
        info0("Policy mode GREEDY");
    }

    //Initialize shared memory
    ConfigShMem(default_cpus, greedy, spd->options.shm_key);

    if (spd->options.lewi_warmup) {
        setThreads_Lend_light(&spd->pm, mu_get_system_size());
        setThreads_Lend_light(&spd->pm, default_cpus);
    }

    enabled = 1;

    return DLB_SUCCESS;
}

int lewi_Finalize(subprocess_descriptor_t *spd) {
    finalize_comm();
    return DLB_SUCCESS;
}

int lewi_EnableDLB(const subprocess_descriptor_t *spd) {
    single = 0;
    enabled = 1;
    return DLB_SUCCESS;
}

int lewi_DisableDLB(const subprocess_descriptor_t *spd) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "ResetDLB");
        acquireCpus(myCPUS);
        setThreads_Lend_light(&spd->pm, default_cpus);
    }
    enabled = 0;
    return DLB_SUCCESS;
}

int lewi_SetMaxParallelism(const subprocess_descriptor_t *spd, int max) {
    if (myCPUS>max){
        releaseCpus(myCPUS-max);
        setThreads_Lend_light(&spd->pm, max);
    }
    max_parallelism = max;
    return DLB_SUCCESS;
}

int lewi_UnsetMaxParallelism(const subprocess_descriptor_t *spd) {
    max_parallelism = 0;
    return DLB_SUCCESS;
}

int lewi_IntoCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS; }

int lewi_OutOfCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS;}

int lewi_IntoBlockingCall(const subprocess_descriptor_t *spd) {

    if (enabled) {
        if ( spd->options.lewi_keep_cpu_on_blocking_call ) {
            /* 1CPU */
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS-1);
            releaseCpus(myCPUS-1);
            setThreads_Lend_light(&spd->pm, 1);
        } else {
            /* BLOCK */
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS);
            releaseCpus(myCPUS);
            setThreads_Lend_light(&spd->pm, 0);
        }
    }
    return DLB_SUCCESS;
}

int lewi_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {

    if (enabled) {
        int cpus;
        if (single) {
            cpus = acquireCpus(1);
        } else {
            cpus = acquireCpus(myCPUS);
        }
        setThreads_Lend_light(&spd->pm, cpus);
        verbose(VB_MICROLB, "ACQUIRING %d cpus", cpus);
    }
    return DLB_SUCCESS;
}

int lewi_Lend(const subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "LENDING %d cpus", myCPUS-1);
    releaseCpus(myCPUS-1);
    setThreads_Lend_light(&spd->pm, 1);
    return DLB_SUCCESS;
}

int lewi_Reclaim(const subprocess_descriptor_t *spd) {
    int cpus = acquireCpus(myCPUS);
    setThreads_Lend_light(&spd->pm, cpus);
    verbose(VB_MICROLB, "ACQUIRING %d cpus", cpus);
    return DLB_SUCCESS;
}

int lewi_Borrow(const subprocess_descriptor_t *spd) {
    return lewi_BorrowCpus(spd, INT_MAX);
}

int lewi_BorrowCpus(const subprocess_descriptor_t *spd, int maxResources) {
    int error = DLB_NOUPDT;
    if (enabled && !single) {
        int max_resources = max_parallelism > 0 ?
            min_int(maxResources, max_parallelism - myCPUS) : maxResources;
        int cpus = checkIdleCpus(myCPUS, max_resources);
        if (myCPUS!=cpus) {
            verbose(VB_MICROLB, "Using %d cpus", cpus);
            setThreads_Lend_light(&spd->pm, cpus);
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

/******* Auxiliar Functions LeWI Balancing Policy ********/
static void setThreads_Lend_light(const pm_interface_t *pm, int numThreads) {

    if (myCPUS!=numThreads) {
        verbose(VB_MICROLB, "Using %d cpus", numThreads);
        update_threads(pm, numThreads);
        myCPUS=numThreads;
    }
}
