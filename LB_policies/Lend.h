#ifndef LEND_H
#define LEND_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Lend Balancing Policy ********/

void Lend_Init(int me, int num_procs, int node);

void Lend_Finish(void);

void Lend_InitIteration();

void Lend_FinishIteration(double cpuSecs, double MPISecs);

void Lend_IntoCommunication(void);

void Lend_OutOfCommunication(void);

void Lend_IntoBlockingCall(double cpuSecs, double MPISecs);

void Lend_OutOfBlockingCall(void);

/******* Auxiliar Functions Lend Balancing Policy ********/

int createThreads_Lend();

void* masterThread_Lend(void* arg);
void applyNewDistribution(int* cpus, int except);
int CalculateNewDistribution_Lend(int* cpus, int process, ProcMetrics info[]);
int CalculateNewDistribution_Lend_siguiente(int* cpus, int process, ProcMetrics info);

void* slaveThread_Lend(void* arg);
void Lend_updateresources();
void setThreads_Lend(int numThreads);

#endif //LEND_H

