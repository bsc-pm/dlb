/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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
#include "LB_numThreads/numThreads.h"
#include "LB_comm/comm_lend_light.h"
#include "support/globals.h"
#include "support/mask_utils.h"
#include "support/utils.h"
#include "support/debug.h"

static int default_cpus;
static int myCPUS = 0;
static int enabled = 0;
static int single = 0;

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init() {
    verbose(VB_MICROLB, "Lend_light Init");

    _process_id = _process_id;
    _node_id = _node_id;
    default_cpus = _default_nthreads;

    setThreads_Lend_light(default_cpus);

    info0("Default cpus per process: %d", default_cpus);

    bool bind;
    parse_env_bool("LB_BIND", &bind, false);
    if (bind) {
        // FIXME: Not implemented
        info0("Binding of threads to cpus enabled");
    }

    bool greedy;
    parse_env_bool("LB_GREEDY", &greedy, false);
    if (greedy) {
        info0("Policy mode GREEDY");
    }

    //Initialize shared _process_idmory
    ConfigShMem(_mpis_per_node, _process_id, _node_id, default_cpus, greedy);

    if ( _aggressive_init ) {
        setThreads_Lend_light( mu_get_system_size() );
        setThreads_Lend_light( _default_nthreads );
    }

    enabled = 1;
}

void Lend_light_Finish(void) {
    finalize_comm();
}

void Lend_light_IntoCommunication(void) {}

void Lend_light_OutOfCommunication(void) {}

void Lend_light_IntoBlockingCall(int is_iter, int blocking_mode) {

    if (enabled) {
        if ( blocking_mode == ONE_CPU ) {
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS-1);
            releaseCpus(myCPUS-1);
            setThreads_Lend_light(1);
        } else {
            verbose(VB_MICROLB, "LENDING %d cpus", myCPUS);
            releaseCpus(myCPUS);
            setThreads_Lend_light(0);
        }
    }
}

void Lend_light_OutOfBlockingCall(int is_iter) {

    if (enabled) {
        int cpus;
        if (single) {
            cpus = acquireCpus(1);
        } else {
            cpus = acquireCpus(myCPUS);
        }
        setThreads_Lend_light(cpus);
        verbose(VB_MICROLB, "ACQUIRING %d cpus", cpus);
    }
}

void Lend_light_updateresources(int maxResources) {
    if (enabled && !single) {
        int cpus = checkIdleCpus(myCPUS, maxResources);
        if (myCPUS!=cpus) {
            verbose(VB_MICROLB, "Using %d cpus", cpus);
            setThreads_Lend_light(cpus);
        }
    }
}

void Lend_light_resetDLB(void) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "ResetDLB");
        acquireCpus(myCPUS);
        setThreads_Lend_light(default_cpus);
    }
}

void Lend_light_disableDLB(void) {
    Lend_light_resetDLB();
    enabled = 0;
}

void Lend_light_enableDLB(void) {
    single = 0;
    enabled = 1;
}

void Lend_light_single(void) {
    Lend_light_IntoBlockingCall(0, ONE_CPU);
    single = 1;
}

void Lend_light_parallel(void) {
    Lend_light_OutOfBlockingCall(0);
    single = 0;
}

/******* Auxiliar Functions Lend_light Balancing Policy ********/
void setThreads_Lend_light(int numThreads) {

    if (myCPUS!=numThreads) {
        verbose(VB_MICROLB, "Using %d cpus", numThreads);
        update_threads(numThreads);
        myCPUS=numThreads;
    }
}
