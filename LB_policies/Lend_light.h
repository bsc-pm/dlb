#ifndef LEND_LIGHT_H
#define LEND_LIGHT_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(int me, int num_procs, int node);

void Lend_light_Finish(void);

void Lend_light_InitIteration();

void Lend_light_FinishIteration(double cpuSecs, double MPISecs);

void Lend_light_IntoCommunication(void);

void Lend_light_OutOfCommunication(void);

void Lend_light_IntoBlockingCall(double cpuSecs, double MPISecs);

void Lend_light_OutOfBlockingCall(void);

void Lend_light_IntoSequentialCode(void);

void Lend_light_OutOfSequentialCode(void);

/******* Auxiliar Functions Lend_light Balancing Policy ********/

int createThreads_Lend_light();

void* masterThread_Lend_light(void* arg);
void applyNewDistribution_light(int* cpus, int except);
int CalculateNewDistribution_Lend_light(int* cpus, int process, ProcMetrics info[]);
int CalculateNewDistribution_Lend_light_siguiente(int* cpus, int process, ProcMetrics info);

void* slaveThread_Lend_light(void* arg);
void Lend_light_updateresources();
void setThreads_Lend_light(int numThreads);

#endif //LEND_H

