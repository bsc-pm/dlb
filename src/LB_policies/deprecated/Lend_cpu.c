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

#include <Lend_cpu.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_lend_light.h>
#include <LB_MPI/tracing.h>
#include "support/globals.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/resource.h>

int me, node, procs;
int greedy;
int default_cpus;
int myCPUS;
int block;

/* Blocking modes */
#define _1CPU 0 // MPI not set to blocking, leave a cpu while in a MPI blockin call
#define MPI_BLOCKING 1 //MPI set to blocking mode
#define PRIO 2 // MPI not set to blocking, decrease priority when in a blocking call

/******* Main Functions Lend_cpu Balancing Policy ********/

void Lend_cpu_Init(int meId, int num_procs, int nodeId) {
#ifdef debugConfig
    fprintf(stderr, "DLB DEBUG: (%d:%d) - Lend_cpu Init\n", nodeId, meId);
#endif
    char* policy_greedy;
    char* blocking;
    me = meId;
    node = nodeId;
    procs = num_procs;
    default_cpus = _default_nthreads;

    setThreads_Lend_cpu(default_cpus);
//  myCPUS=default_cpus;

#ifdef debugBasicInfo
    if (me==0 && node==0) {
        fprintf(stdout, "DLB: Default cpus per process: %d\n", default_cpus);
    }
#endif

    greedy=0;
    if ((policy_greedy=getenv("LB_GREEDY"))!=NULL) {
        if(strcasecmp(policy_greedy, "YES")==0) {
            greedy=1;
#ifdef debugBasicInfo
            if (me==0 && node==0) {
                fprintf(stdout, "DLB: Policy mode GREEDY\n");
            }
#endif
        }
    }

//Setting blocking mode
    block=_1CPU;
    if ((blocking=getenv("MXMPI_RECV"))!=NULL) {
        if (strcasecmp(blocking, "blocking")==0) {
            block=MPI_BLOCKING;
#ifdef debugBasicInfo
            fprintf(stdout, "DLB: (%d:%d) - MPI blocking mode set to blocking (I will lend all the resources)\n", node, me);
#endif
        }
    }
    if(block==_1CPU) {
#ifdef debugBasicInfo
        fprintf(stderr, "DLB: (%d:%d) - MPI blocking mode NOT set to blocking\n", node, me);
#endif
        if ((blocking=getenv("LB_LEND_MODE"))!=NULL) {
            if (strcasecmp(blocking, "PRIO")==0) {
                block=PRIO;
#ifdef debugBasicInfo
                fprintf(stderr, "DLB (%d:%d) - LEND mode set to PRIO. I will decrease priority when in an MPI call\n", node, me);
#endif
            }
        }
    }
#ifdef debugBasicInfo
    if (block==_1CPU) {
        fprintf(stderr, "DLB: (%d:%d) - LEND mode set to 1CPU. I will leave a cpu per MPI process when in an MPI call\n", node, me);
    }
#endif

    //Initialize shared memory
    ConfigShMem(procs, me, node, default_cpus, greedy);

}

void Lend_cpu_Finish(void) {
    if (me==0) { finalize_comm(); }
}

void Lend_cpu_InitIteration() {}

void Lend_cpu_FinishIteration(double cpuSecs, double MPISecs) {}

void Lend_cpu_IntoCommunication(void) {}

void Lend_cpu_OutOfCommunication(void) {}

void Lend_cpu_IntoBlockingCall(double cpuSecs, double MPISecs) {
    int prio;
    if (block==_1CPU) {
#ifdef debugLend
        fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS-1);
#endif
        releaseCpus(myCPUS-1);
        setThreads_Lend_cpu(1);
    } else {
#ifdef debugLend
        fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS);
#endif
        releaseCpus(myCPUS);
        setThreads_Lend_cpu(0);
        if (block==PRIO) {
            fprintf(stderr,"Current priority %d\n", getpriority(PRIO_PROCESS, 0));
            if((prio=nice(19))<0) { perror("Error setting priority 'nice'"); }
            else { fprintf(stderr,"%d:%d - Priority set to %d\n", node, me, prio); }
        }
    }
}

void Lend_cpu_OutOfBlockingCall(void) {
    int prio;

    if (block==PRIO) {
        fprintf(stderr,"Current priority %d\n", getpriority(PRIO_PROCESS, 0));

        if((prio=nice(-1))<0) { perror("Error setting priority 'nice'"); }
        else { fprintf(stderr,"%d:%d - Priority set to %d\n", node, me, prio); }
    }

    int cpus=acquireCpus(myCPUS);
    setThreads_Lend_cpu(cpus);
#ifdef debugLend
    fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", node, me, cpus);
#endif

}

/******* Auxiliar Functions Lend_cpu Balancing Policy ********/

void Lend_cpu_updateresources() {
    int cpus = checkIdleCpus(myCPUS);


    if (myCPUS!=cpus) {
#ifdef debugDistribution
        fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, cpus);
#endif
        setThreads_Lend_cpu(cpus);
    }
}

void setThreads_Lend_cpu(int numThreads) {

    if (myCPUS!=numThreads) {
#ifdef debugDistribution
        fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, numThreads);
#endif
        update_threads(numThreads);
        myCPUS=numThreads;
    }
}
