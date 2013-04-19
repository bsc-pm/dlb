#ifndef COMM_H
#define COMM_H

#include <netdb.h>

#include <semaphore.h>

typedef struct{
	int action;
	int cpus;
	double secsComp;
	double secsMPI;
}ProcMetrics;

/*typedef struct{
	int proc;
	int data;
}msg_LEND;*/

/*typedef struct{
	int proc;
	ProcMetrics data;
}msg;*/

typedef struct {
	sem_t msg4master;
//	sem_t lock_data[];
}sharedData;

/* Shared Memory structure

----------------------------> struct sharedData
int first
int last
sem_t  msg4master
sem_t  lock_data
----------------------------> 1 queue msgs
msg msg1
msg msg2
...
----------------------------> 2 list msg4slave
sem_t msg4slave1
sem_t msg4slave1
...
----------------------------> 3 list threads
int thread1
int thread2
----------------------------

size = sharedData + msg*procs + sem_t*procs + int*procs
*/

void LoadCommConfig(int num_procs, int meId, int nodeId);
void StartMasterComm();
void StartSlaveComm();
void comm_close();

int GetFromAnySlave(char *info,int size);
void GetFromMaster(char *info,int size);
void SendToMaster(char *info,int size);
void GetFromSlave(int rank,char *info,int size);
void SendToSlave(int rank,char *info,int size);
void GetFromMasterNonBlocking(char *info,int size);
void WriteToSlave(int rank,char *info,int size);

#endif //COMM_H