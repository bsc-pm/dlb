/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
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

#include <DWB_Eco.h>
#include "LB_numThreads/numThreads.h"
#include "support/globals.h"

#include <semaphore.h>
#include <stdio.h>


int me, node, procs;
int finished;
int threads2use, threadsUsed;
ProcMetrics localMetrics;

/******* Main Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_Init(int meId, int num_procs, int nodeId) {
#ifdef debugConfig
    fprintf(stderr, "%d:%d - DWB_Eco Init\n", nodeId, meId);
#endif
    me = meId;
    node = nodeId;
    procs = num_procs;
    threadsUsed=_default_nthreads;
}

void DWB_Eco_Finish(void) {}

void DWB_Eco_InitIteration(void) {}

void DWB_Eco_FinishIteration(double cpuSecs, double MPISecs) {

    double n;
    int x;
    /*  ProcMetrics pm;
        pm.secsComp=cpuSecs;
        pm.secsMPI=MPISecs;
        pm.cpus=threadsUsed;*/

    n = ((float)threadsUsed*cpuSecs)/(cpuSecs+MPISecs);
    x = (n+1);
    if(x<1) { x=1; }
    if(x>(CPUS_NODE-0.5)) { x=CPUS_NODE; }
    update_threads(x);
    if(threadsUsed!=x) { fprintf(stderr, "%d:%d n=%f x=%d\n", node, me, n, x); }
    threadsUsed=x;



    //SendLocalMetrics_DWB_Eco(pm);

}

void DWB_Eco_IntoCommunication(void) {}

void DWB_Eco_OutOfCommunication(void) {
    //updateresources();
}

void DWB_Eco_IntoBlockingCall(double cpuSecs, double MPISecs) {}

void DWB_Eco_OutOfBlockingCall(void) {}

/******* Auxiliar Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_updateresources() {
    /*if (threadsUsed!=threads2use){
    #ifdef debugDistribution
        fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, threads2use);
    #endif
        update_threads(threads2use);
        threadsUsed=threads2use;
    }*/
}

