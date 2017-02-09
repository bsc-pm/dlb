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


#include "LB_policies/Lend_light.h"

#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"

#ifdef MPI_LIB

#include "LB_comm/comm_shMem.h"
#include "LB_comm/comm_lend_light.h"
#include "LB_numThreads/numThreads.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "LB_MPI/process_MPI.h"

#include <sched.h>


static int default_cpus;
static int myCPUS = 0;
static int enabled = 0;
static int single = 0;
static void setThreads_Lend_light(const pm_interface_t *pm, int numThreads);

/******* Main Functions Lend_light Balancing Policy ********/

int Lend_light_Init(const subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "Lend_light Init");

    default_cpus = CPU_COUNT(&spd->process_mask);

    setThreads_Lend_light(&spd->pm, default_cpus);

    info0("Default cpus per process: %d", default_cpus);

    bool greedy = spd->options.greedy;
    if (greedy) {
        info0("Policy mode GREEDY");
    }

    //Initialize shared memory
    ConfigShMem(_mpis_per_node, _process_id, _node_id, default_cpus, greedy, spd->options.shm_key);

    if (spd->options.aggressive_init) {
        setThreads_Lend_light(&spd->pm, mu_get_system_size());
        setThreads_Lend_light(&spd->pm, default_cpus);
    }

    enabled = 1;

    return DLB_SUCCESS;
}

int Lend_light_Finish(const subprocess_descriptor_t *spd) {
    finalize_comm();
    return DLB_SUCCESS;
}

int Lend_light_enableDLB(const subprocess_descriptor_t *spd) {
    single = 0;
    enabled = 1;
    return DLB_SUCCESS;
}

int Lend_light_disableDLB(const subprocess_descriptor_t *spd) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "ResetDLB");
        acquireCpus(myCPUS);
        setThreads_Lend_light(&spd->pm, default_cpus);
    }
    enabled = 0;
    return DLB_SUCCESS;
}

void Lend_light_IntoCommunication(const subprocess_descriptor_t *spd) {}

void Lend_light_OutOfCommunication(const subprocess_descriptor_t *spd) {}

void Lend_light_IntoBlockingCall(const subprocess_descriptor_t *spd) {

    if (enabled) {
        if ( spd->options.mpi_lend_mode == ONE_CPU ) {
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS-1);
            releaseCpus(myCPUS-1);
            setThreads_Lend_light(&spd->pm, 1);
        } else {
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS);
            releaseCpus(myCPUS);
            setThreads_Lend_light(&spd->pm, 0);
        }
    }
}

void Lend_light_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {

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
}

int Lend_light_updateresources(const subprocess_descriptor_t *spd, int maxResources) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        int cpus = checkIdleCpus(myCPUS, maxResources);
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

/******* Auxiliar Functions Lend_light Balancing Policy ********/
static void setThreads_Lend_light(const pm_interface_t *pm, int numThreads) {

    if (myCPUS!=numThreads) {
        verbose(VB_MICROLB, "Using %d cpus", numThreads);
        update_threads(pm, numThreads);
        myCPUS=numThreads;
    }
}

#else /* MPI_LIB */

int Lend_light_Init(const subprocess_descriptor_t *spd) {
    fatal("Lend light policy can only be used if DLB intercepts MPI");
    return DLB_ERR_NOCOMP;
}
int Lend_light_Finish(const subprocess_descriptor_t *spd) {return DLB_ERR_NOCOMP;}
int Lend_light_enableDLB(const subprocess_descriptor_t *spd) {return DLB_ERR_NOCOMP;}
int Lend_light_disableDLB(const subprocess_descriptor_t *spd) {return DLB_ERR_NOCOMP;}
void Lend_light_IntoCommunication(const subprocess_descriptor_t *spd) {}
void Lend_light_OutOfCommunication(const subprocess_descriptor_t *spd) {}
void Lend_light_IntoBlockingCall(const subprocess_descriptor_t *spd) {}
void Lend_light_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {}
int Lend_light_updateresources(const subprocess_descriptor_t *spd, int maxResources) {
    return DLB_ERR_NOCOMP;
}

#endif /* MPI_LIB */
