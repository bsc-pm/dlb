/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Lewi_map.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_map.h>
#include "support/globals.h"
#include "support/tracing.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/resource.h>

int default_cpus;
int myCPUS=0;
int *my_cpus;

/******* Main Functions Map Balancing Policy ********/

void Map_Init() {
#ifdef debugConfig
    fprintf(stderr, "DLB DEBUG: (%d:%d) - Map Init\n", _node_id, _process_id);
#endif
    my_cpus= malloc(sizeof(int)*CPUS_NODE);
    default_cpus = _default_nthreads;

#ifdef debugBasicInfo
    if (_process_id==0 && _node_id==0) {
        fprintf(stdout, "DLB: Default cpus per process: %d\n", default_cpus);
    }
#endif

    //Initialize shared _process_idmory
    ConfigShMem_Map(_mpis_per_node, _process_id, _node_id, default_cpus, my_cpus);
//fprintf(stderr, "DLB: (%d:%d) - setting threads %d (%d, %d)\n", _node_id, _process_id, default_cpus, my_cpus[0], my_cpus[23]);
    setThreads_Map(default_cpus, 1, my_cpus);

}

void Map_Finish(void) {
    free(my_cpus);
    if (_process_id==0) { finalize_comm_Map(); }
}

void Map_IntoCommunication(void) {}

void Map_OutOfCommunication(void) {}

void Map_IntoBlockingCall(int is_iter, int blocking_mode) {

    int res;

    if ( blocking_mode == ONE_CPU ) {
#ifdef debugLend
        fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", _node_id, _process_id, myCPUS-1);
#endif
        res=myCPUS;
        setThreads_Map(1, 3, my_cpus);
        res=releaseCpus_Map(res-1, my_cpus);
    } else {
#ifdef debugLend
        fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", _node_id, _process_id, myCPUS);
#endif
        res=myCPUS;
        setThreads_Map(0, 3, my_cpus);
        res=releaseCpus_Map(res, my_cpus);
    }
}

void Map_OutOfBlockingCall(int is_iter) {

    int cpus=acquireCpus_Map(myCPUS, my_cpus);
    setThreads_Map(cpus, 1, my_cpus);
#ifdef debugLend
    fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", _node_id, _process_id, cpus);
#endif

}

/******* Auxiliar Functions Map Balancing Policy ********/

void Map_updateresources(int max_cpus) {
    int action;
    int cpus = checkIdleCpus_Map(myCPUS, max_cpus, my_cpus);
    if (myCPUS!=cpus) {
#ifdef debugDistribution
        fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", _node_id, _process_id, cpus);
#endif
        if (cpus>myCPUS) { action=1; }
        if (cpus<myCPUS) { action=2; }
        setThreads_Map(abs(cpus), action, my_cpus);
    }
}

void setThreads_Map(int numThreads, int action, int* cpus) {
//fprintf(stderr, "DLB: (%d:%d) - setting threads inside %d (%d -> %d)\n", _node_id, _process_id, action, myCPUS, numThreads);

    if (myCPUS!=numThreads) {
#ifdef debugDistribution
        fprintf(stderr,"DLB DEBUG: (%d:%d) - I have %d cpus I'm going to use %d cpus\n", _node_id, _process_id, myCPUS, numThreads);
#endif
        add_event(THREADS_USED_EVENT, numThreads);
        int num_cpus=1;

        if (action==1) { num_cpus=numThreads-myCPUS; }
        if (action==2) { num_cpus=myCPUS-numThreads; }
        if (action==3) { num_cpus=myCPUS-numThreads; }
        if ((numThreads==0)||(myCPUS==0)) {
            num_cpus--; //We always leave a cpu to smpss
            cpus=&cpus[1];
        }


        update_cpus(action, num_cpus, cpus);
//fprintf(stderr, "(%d:%d) ******************** CPUS %d --> %d\n", _node_id, _process_id, myCPUS, numThreads);
        myCPUS=numThreads;
    }
}
