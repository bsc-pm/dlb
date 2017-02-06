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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdlib.h>

#include "LB_policies/Lewi_map.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/comm_map.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "LB_core/spd.h"

#ifdef MPI_LIB

#include "LB_MPI/process_MPI.h"

int default_cpus;
int myCPUS=0;
int *my_cpus;

/******* Main Functions Map Balancing Policy ********/

void Map_Init(const cpu_set_t *process_mask) {
    verbose(VB_MICROLB, "Map Init");
    my_cpus= malloc(sizeof(int)*CPUS_NODE);
    default_cpus = CPU_COUNT(process_mask);

    info0("Default cpus per process: %d", default_cpus);

    //Initialize shared memory
    ConfigShMem_Map(_mpis_per_node, _process_id, _node_id, default_cpus, my_cpus,
            global_spd.options.shm_key);
    setThreads_Map(default_cpus, 1, my_cpus);
}

void Map_Finish(void) {
    free(my_cpus);
    finalize_comm_Map();
}

void Map_IntoCommunication(void) {}

void Map_OutOfCommunication(void) {}

void Map_IntoBlockingCall(int is_iter, int blocking_mode) {

    int res;

    if ( blocking_mode == ONE_CPU ) {
        verbose(VB_MICROLB, "LENDING %d cpus", myCPUS-1);
        res=myCPUS;
        setThreads_Map(1, 3, my_cpus);
        res=releaseCpus_Map(res-1, my_cpus);
    } else {
        verbose(VB_MICROLB, "LENDING %d cpus", myCPUS);
        res=myCPUS;
        setThreads_Map(0, 3, my_cpus);
        res=releaseCpus_Map(res, my_cpus);
    }
}

void Map_OutOfBlockingCall(int is_iter) {

    int cpus=acquireCpus_Map(myCPUS, my_cpus);
    setThreads_Map(cpus, 1, my_cpus);
    verbose(VB_MICROLB, "ACQUIRING %d cpus", cpus);
}

/******* Auxiliar Functions Map Balancing Policy ********/

void Map_updateresources(int max_cpus) {
    int action;
    int cpus = checkIdleCpus_Map(myCPUS, max_cpus, my_cpus);
    if (myCPUS!=cpus) {
        verbose(VB_MICROLB, "Using %d cpus", cpus);
        if (cpus>myCPUS) { action=1; }
        if (cpus<myCPUS) { action=2; }
        setThreads_Map(abs(cpus), action, my_cpus);
    }
}

void setThreads_Map(int numThreads, int action, int* cpus) {

    if (myCPUS!=numThreads) {
        verbose(VB_MICROLB, "I have %d cpus I'm going to use %d cpus", myCPUS, numThreads);
        add_event(THREADS_USED_EVENT, numThreads);
        int num_cpus=1;

        if (action==1) { num_cpus=numThreads-myCPUS; }
        if (action==2) { num_cpus=myCPUS-numThreads; }
        if (action==3) { num_cpus=myCPUS-numThreads; }
        if ((numThreads==0)||(myCPUS==0)) {
            num_cpus--; //We always leave a cpu to smpss
            cpus=&cpus[1];
        }


        // Not implemented
        //update_cpus(action, num_cpus, cpus);
//fprintf(stderr, "(%d:%d) ******************** CPUS %d --> %d\n", _node_id, _process_id, myCPUS, numThreads);
        myCPUS=numThreads;
    }
}

#else /* MPI_LIB */

void Map_Init(const cpu_set_t *process_mask) {
    fatal("Map policy can only be used if DLB intercepts MPI");
}
void Map_Finish(void) {}
void Map_IntoCommunication(void) {}
void Map_OutOfCommunication(void) {}
void Map_IntoBlockingCall(int is_iter, int blocking_mode) {}
void Map_OutOfBlockingCall(int is_iter) {}
void Map_updateresources(int max_cpus) {}

#endif /* MPI_LIB */
