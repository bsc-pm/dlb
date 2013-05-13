#ifndef DWB_Eco_H
#define DWB_Eco_H

#include <LB_comm/comm.h>

/******* Main Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_Init(int meId, int num_procs, int nodeId);

void DWB_Eco_Finish(void);

void DWB_Eco_InitIteration(void);

void DWB_Eco_FinishIteration(double cpuSecs, double MPISecs);

void DWB_Eco_IntoCommunication(void);

void DWB_Eco_OutOfCommunication(void);

void DWB_Eco_IntoBlockingCall(double cpuSecs, double MPISecs);

void DWB_Eco_OutOfBlockingCall(void);

void DWB_Eco_updateresources();

/******* Auxiliar Functions DWB_Eco Balancing Policy ********/

void* masterThread_DWB_Eco(void* arg);
void* slaveThread_DWB_Eco(void* arg);

void GetMetrics_DWB_Eco(ProcMetrics metrics[]);
//void getLocaMetrics(ProcMetrics *metr);
void SendLocalMetrics_DWB_Eco(ProcMetrics LM);
void CalculateNewDistribution_DWB_Eco(ProcMetrics LM[], int cpus[]);
void applyNewDistribution_DWB_Eco(int cpus[]);

#endif //DWB_Eco_H

