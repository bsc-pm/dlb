#ifndef WEIGHT_H
#define WEIGHT_H


#include <LB_comm/comm.h>
/******* Main Functions Weight Balancing Policy ********/

void Weight_Init(int meId, int num_procs, int nodeId);

void Weight_Finish(void);

void Weight_IntoCommunication(void);

void Weight_OutOfCommunication(void);

void Weight_IntoBlockingCall(int is_iter);

void Weight_OutOfBlockingCall(int is_iter);

void Weight_updateresources();

/******* Auxiliar Functions Weight Balancing Policy ********/

void* masterThread_Weight(void* arg);
void* slaveThread_Weight(void* arg);

void createThreads_Weight();

void GetMetrics(ProcMetrics metrics[]);
void getLocaMetrics(ProcMetrics *metr);
void SendLocalMetrics(ProcMetrics LM);
void CalculateNewDistribution_Weight(ProcMetrics LM[], int cpus[]);
void applyNewDistribution_Weight(int cpus[]);

#endif //WEIGHT_H

