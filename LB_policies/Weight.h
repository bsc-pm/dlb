#ifndef WEIGHT_H
#define WEIGHT_H

#include <LB_comm/comm.h>

/******* Main Functions Weight Balancing Policy ********/

void Weight_Init(int meId, int num_procs, int nodeId);

void Weight_Finish(void);

void Weight_InitIteration(void);

void Weight_FinishIteration(double cpuSecs, double MPISecs);

void Weight_IntoCommunication(void);

void Weight_OutOfCommunication(void);

void Weight_IntoBlockingCall(double cpuSecs, double MPISecs);

void Weight_OutOfBlockingCall(void);

void Weight_updateresources();

/******* Auxiliar Functions Weight Balancing Policy ********/

void* masterThread_Weight(void* arg);
void* slaveThread_Weight(void* arg);

void GetMetrics(ProcMetrics metrics[]);
void getLocaMetrics(ProcMetrics *metr);
void SendLocalMetrics(ProcMetrics LM);
void CalculateNewDistribution_Weight(ProcMetrics LM[], int cpus[]);
void applyNewDistribution_Weight(int cpus[]);

#endif //WEIGHT_H