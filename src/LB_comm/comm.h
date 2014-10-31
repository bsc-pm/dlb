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

#ifndef COMM_H
#define COMM_H

#include <netdb.h>

#include <semaphore.h>

typedef struct {
    int action;
    int cpus;
    double secsComp;
    double secsMPI;
} ProcMetrics;

/*typedef struct{
    int proc;
    int data;
}msg_LEND;*/

typedef struct {
    int proc;
    ProcMetrics data;
} msg;

typedef struct {
    int first;
    int last;
    sem_t msg4master;
    sem_t lock_data;
    sem_t queue;
} sharedData;

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

