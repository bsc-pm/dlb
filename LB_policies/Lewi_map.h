#ifndef LEND_MAP_H
#define LEND_MAP_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Map Balancing Policy ********/

void Map_Init(int me, int num_procs, int node);

void Map_Finish(void);

void Map_InitIteration();

void Map_FinishIteration(double cpuSecs, double MPISecs);

void Map_IntoCommunication(void);

void Map_OutOfCommunication(void);

void Map_IntoBlockingCall(double cpuSecs, double MPISecs);

void Map_OutOfBlockingCall(void);

/******* Auxiliar Functions Map Balancing Policy ********/

void Map_updateresources(int max_cpus);
void setThreads_Map(int numThreads, int action, int* cpus);

#endif //LEND_H

