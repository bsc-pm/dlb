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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_numThreads/numThreads.h"
#include "LB_policies/Weight.h"
#include "support/globals.h"
#include "support/debug.h"
#include "support/utils.h"
#include "support/mytime.h"
#include "support/tracing.h"

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int finished;
int threads2use, threadsUsed;
static sem_t sem_localMetrics;
ProcMetrics localMetrics;

struct timespec initAppl;
struct timespec initComp;
struct timespec initMPI;

struct timespec iterCpuTime;
struct timespec iterMPITime;

struct timespec CpuTime;
struct timespec MPITime;

int iterNum;
/******* Main Functions Weight Balancing Policy ********/

void Weight_Init(void) {
    verbose(VB_MICROLB, "Weight Init");

    clock_gettime(CLOCK_REALTIME, &initAppl);
    reset(&iterCpuTime);
    reset(&iterMPITime);
    reset(&CpuTime);
    reset(&MPITime);
    clock_gettime(CLOCK_REALTIME, &initComp);

    iterNum=0;
    //Create auxiliar threads
    createThreads_Weight();
}

void Weight_Finish(void) {
    comm_close();
}

void Weight_IntoCommunication(void) {}

void Weight_OutOfCommunication(void) {
    Weight_updateresources();
}

void Weight_IntoBlockingCall(int is_iter, int blocking_mode) {
    struct timespec aux;
    clock_gettime(CLOCK_REALTIME, &initMPI);
    diff_time(initComp, initMPI, &aux);
    add_time(iterCpuTime, aux, &iterCpuTime);
}

void Weight_OutOfBlockingCall(int is_iter) {
    struct timespec aux;
    clock_gettime(CLOCK_REALTIME, &initComp);
    diff_time(initMPI, initComp, &aux);
    add_time(iterMPITime, aux, &iterMPITime);


    if (is_iter!=0) {
        if(iterNum!=0) {
            //Finishing iteration
            add_event(ITERATION_EVENT, 0);

            double cpuSecs;
            double MPISecs;

            cpuSecs=to_secs(iterCpuTime);
            MPISecs=to_secs(iterMPITime);

            ProcMetrics pm;
            pm.action=0;
            pm.secsComp=cpuSecs;
            pm.secsMPI=MPISecs;
            pm.cpus=threadsUsed;

            SendLocalMetrics(pm);
        }
        iterNum++;
        add_event(ITERATION_EVENT, iterNum);

        add_time(CpuTime, iterCpuTime, &CpuTime);
        add_time(MPITime, iterMPITime, &MPITime);

        reset(&iterCpuTime);
        reset(&iterMPITime);
    }
}

/******* Auxiliar Functions Weight Balancing Policy ********/

/* Creates auxiliar threads */
void createThreads_Weight() {
    pthread_t t;
    finished=0;

    verbose(VB_MICROLB, "Creating Threads");

    threadsUsed=0;
    threads2use=_default_nthreads;

    //The local thread won't communicate by sockets
    LoadCommConfig(_mpis_per_node, _process_id, _node_id);

    if (_process_id==0) {
        if (pthread_create(&t,NULL,masterThread_Weight,NULL)>0) {
            perror("DLB PANIC: createThreads_Weight:Error in pthread_create master\n");
            exit(1);
        }
    }

    if(pthread_create(&t,NULL,slaveThread_Weight,NULL)>0) {
        perror("DLB PANIC: createThreads_Weight:Error in pthread_create slave\n");
        exit(1);
    }

    Weight_updateresources();
}

/******* Master Thread Functions ********/
void* masterThread_Weight(void* arg) {

    int cpus[_mpis_per_node];
    int i;

    verbose(VB_MICROLB, "Creating Master thread");

    StartMasterComm();

    ProcMetrics currMetrics[_mpis_per_node];

    if (sem_init(&sem_localMetrics,0,0)<0) {
        perror("DLB PANIC: Initializing metrics semaphore\n");
        exit(1);
    }

    //We start with equidistribution
    for (i=0; i<_mpis_per_node; i++) { cpus[i]=_default_nthreads; }

//  applyNewDistribution_Weight(cpus);

    while(!finished) {
        GetMetrics(currMetrics);
        CalculateNewDistribution_Weight(currMetrics, cpus);
        applyNewDistribution_Weight(cpus);
    }
    return NULL;
}

void GetMetrics(ProcMetrics metrics[]) {
    int i, slave;
    ProcMetrics metr;

    /*getLocaMetrics(&metr);
    metrics[0]=metr;*/

    for (i=0; i<_mpis_per_node; i++) {
        slave=GetFromAnySlave((char *) &metr,sizeof(ProcMetrics));
        metrics[slave]=metr;
    }
}

void getLocaMetrics(ProcMetrics *metr) {
    sem_wait(&sem_localMetrics);
    *metr=localMetrics;
}

void SendLocalMetrics(ProcMetrics LM) {
    localMetrics = LM;
    sem_post(&sem_localMetrics);
}

void CalculateNewDistribution_Weight(ProcMetrics LM[], int cpus[]) {
    double total_time=0;
    int i, cpus_alloc, total_cpus=0;
    double weights[_mpis_per_node];
//make it global
    double weight_1cpu = 100/CPUS_NODE;
    int cpus_to_give=CPUS_NODE-_mpis_per_node;
    double total_weight=0;

    //Calculate total runtime
    for (i=0; i<_mpis_per_node; i++) {
        total_time+=(LM[i].secsComp * LM[i].cpus);
    }

    //Calculate weigth per process
    for (i=0; i<_mpis_per_node; i++) {
        weights[i]=(double)((LM[i].secsComp * (double)LM[i].cpus *(double)100/(double)total_time)-weight_1cpu);
        if (weights[i]>0) {
            total_weight+=weights[i];
        }
//#ifdef debugLoads
        fprintf(stderr,"DLB DEBUG: [Process %d] Comp. time: %f - Load: %f\n", i, LM[i].secsComp, weights[i]);
//#endif
    }
//#ifdef debugDistribution
    fprintf(stderr,"DLB DEBUG: New Distribution: ");
//#endif
    //Calculate new distribution
    for (i=0; i<_mpis_per_node; i++) {
        //By default one cpu per process
        cpus_alloc=1;
        //Then we distribute the remaning ones
        cpus_alloc+=my_round((weights[i]*(double)cpus_to_give)/(double)total_weight);


//      if (cpus_alloc>(CPUS_NODE-(_mpis_per_node-1))) cpus[i]=CPUS_NODE-(_mpis_per_node-1);
//      else if (cpus_alloc<1) cpus[i]=1;
//      else
        cpus[i]=cpus_alloc;

//      #ifdef debugDistribution
        fprintf(stderr,"[%d]", cpus[i]);
//      #endif
        total_cpus+=cpus[i];
    }
//#ifdef debugDistribution
    fprintf(stderr,"\n");
//#endif
    if (total_cpus>CPUS_NODE) {
        fprintf(stderr,"DLB WARNING: Using more cpus than the ones available in the _node_id (%d>%d)\n", total_cpus, CPUS_NODE);
    } else if(total_cpus<CPUS_NODE) {
        fprintf(stderr,"DLB WARNING: Using less cpus than the ones available in the _node_id (%d<%d)\n", total_cpus, CPUS_NODE);
    }
}

void applyNewDistribution_Weight(int cpus[]) {
    int i;
    //threads2use = cpus[0];
    for (i=0; i<_mpis_per_node; i++) {
        SendToSlave(i, (char*)&cpus[i], sizeof(int));
    }
}

/******* Slave Thread Functions ********/
void* slaveThread_Weight(void* arg) {
    int cpus;
    ProcMetrics metr;

    if (sem_init(&sem_localMetrics,0,0)<0) {
        perror("DLB PANIC: Initializing metrics semaphore\n");
        exit(1);
    }

    verbose(VB_MICROLB, "Creating Slave thread");
    StartSlaveComm();

    while(!finished) {
        getLocaMetrics(&metr);
        SendToMaster((char*)&metr, sizeof(ProcMetrics));
        GetFromMaster((char*)&cpus,sizeof(int));
        threads2use=cpus;
    }
    return NULL;
}

void Weight_updateresources() {
    if (threadsUsed!=threads2use) {
        verbose(VB_MICROLB, "Using %d cpus", threads2use);
        update_threads(threads2use);
        threadsUsed=threads2use;
    }
}
