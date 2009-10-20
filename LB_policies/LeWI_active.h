#ifndef LEWI_A_H
#define LEWI_A_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1
#define ASK_4_CPUS 2

#include <LB_comm/comm.h>

/******* Main Functions LeWI_A Balancing Policy ********/

void LeWI_A_Init(int me, int num_procs, int node);

void LeWI_A_Finish(void);

void LeWI_A_InitIteration();

void LeWI_A_FinishIteration(double cpuSecs, double MPISecs);

void LeWI_A_IntoCommunication(void);

void LeWI_A_OutOfCommunication(void);

void LeWI_A_IntoBlockingCall(double cpuSecs, double MPISecs);

void LeWI_A_OutOfBlockingCall(void);

/******* Auxiliar Functions LeWI_A Balancing Policy ********/

int createThreads_LeWI_A();

void* masterThread_LeWI_A(void* arg);
void LeWI_A_applyNewDistribution(int* cpus, int except);

void* slaveThread_LeWI_A(void* arg);
void LeWI_A_updateresources();
void setThreads_LeWI_A(int numThreads);

#endif //LEWI_A_H