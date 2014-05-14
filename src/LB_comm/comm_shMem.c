/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#include <comm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

int procs;
int me;
int node;

int size_queue;

//pointers to the shared memory structures
sharedData* shData;
msg * msgs;
sem_t * msg4slave;
int * threads;
int shmid;

void LoadCommConfig(int num_procs, int meId, int nodeId){
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) - %d LoadCommonConfig\n", node,me,  getpid());
#endif
	procs=num_procs;
	me=meId;
	node=nodeId;

//	char *k;
	int i;
    key_t key;
    int sm_size;
    char * shm;
	size_queue=procs*100;

	/*if ((k=getenv("SLURM_JOBID"))==NULL){
		key=getppid();
		fprintf(stdout,"DLB: (%d:%d) SLURM_JOBID not found using getppid(%d) as Shared Memory name\n", node, me, key);
	}else{
		key=atol(k);
	}*/

	key=(key_t) 10002;
	
	sm_size= sizeof(sharedData) + (sizeof(msg)*size_queue) + (sizeof(sem_t)*procs)+ (sizeof(int)*procs);
	//         the struct         the queue of messages       the semaphores         the threads    

	if (me==0){
       
	
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Start Master Comm - creating shared mem \n", node, me);
#endif
	
		if ((shmid = shmget(key, sm_size, IPC_EXCL | IPC_CREAT | 0666)) < 0) {
			perror("DLB PANIC: shmget Master");
			exit(1);
		}
	
		if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
			perror("DLB PANIC: shmat Master");
			exit(1);
		}
	
		shData=(sharedData*)shm;
	
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) setting values to the shared mem\n", node, me);
#endif

		//The top and the tail of the queue
		shData->first=0;
		shData->last=0;

		//The semaphore to warn the master of a new message
		if (sem_init(&(shData->msg4master), 1, 0)<0){
			perror("DLB PANIC: sem_init msg4master");
			exit(1);
		}

		//The semaphore to have exclusive acces to the data
		if (sem_init(&(shData->lock_data), 1, 1)<0){
			perror("DLB PANIC: sem_init lock_data");
			exit(1);
		}

		//The semaphore to have space in the queue
		if (sem_init(&(shData->queue), 1, (size_queue-1))<0){
			perror("DLB PANIC: sem_init lock_data");
			exit(1);
		}

		//The queue of messages for the master
		msgs = (msg*)((char*)shData + sizeof(sharedData)); 


		//The array of sempahores to warn the slaves of a change in the number of threads
		msg4slave =  (sem_t*)((char*)msgs + (sizeof(msg)*size_queue)); 
		for (i=0; i<procs; i++){
			if (sem_init(&(msg4slave[i]), 1, 0)<0){
				perror("DLB PANIC: sem_init msg4slave");
				exit(1);
			}
		}	

		//The array with the threads to be used by each slave
		threads = (int*)((char*)msg4slave + (sizeof(sem_t)*procs));
		for (i=0; i<procs; i++){
			threads[i]=0;
		}

#ifdef HAVE_MPI
		PMPI_Barrier(MPI_COMM_WORLD);
#endif

	}else{
#ifdef debugSharedMem 
    	fprintf(stderr,"DLB DEBUG: (%d:%d) Slave Comm - associating to shared mem\n", node, me);
#endif

#ifdef HAVE_MPI
		PMPI_Barrier(MPI_COMM_WORLD);
#endif
	
		
		shmid = shmget(key, sm_size, 0666);
	
		while (shmid<0 && errno==ENOENT){
			shmid = shmget(key, sm_size, 0666);
		}
		if (shmid < 0) {
			perror("DLB PANIC: shmget slave");
			exit(1);
		}
	
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Slave Comm - associated to shared mem\n", node, me);
#endif
		if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
			perror("DLB PANIC: shmat slave");
			exit(1);
		}
	
		shData=(sharedData*)shm;

		//The queue of messages for the master
		msgs = (msg*)((char*)shData + sizeof(sharedData)); 

		//The array of sempahores to warn the slaves of a change in the number of threads
		msg4slave =  (sem_t*)((char*)msgs + (sizeof(msg)*size_queue));
		
		//The array with the threads to be used by each slave
		threads = (int*)((char*)msg4slave + (sizeof(sem_t)*procs));
	}


	

}

void StartMasterComm(){
}
void StartSlaveComm(){
}
void printQueue(){
	int i; 
	fprintf(stderr,"DLB DEBUG: (%d:%d) - ", node, me);
	for (i=0; i<size_queue; i++){
		fprintf(stderr,"[%d,%d]", msgs[i].proc, msgs[i].data.action);
	}
	fprintf(stderr,"\n");
	
	fprintf(stderr,"DLB DEBUG: (%d:%d) - Num Threads", node, me);
	for (i=0; i<procs; i++){
		fprintf(stderr,"[%d]", threads[i]);
	}
	fprintf(stderr,"\n");
}

int GetFromAnySlave(char *info,int size){
	int slave;
	
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Master waiting in semaphore\n", node, me);
#endif
    sem_wait(&(shData->msg4master));
    sem_wait(&(shData->lock_data));

	slave=msgs[shData->first].proc;
	memcpy(info, &(msgs[shData->first].data), size);

	shData->first= (shData->first+1)%size_queue;

#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Master received from %d\n", node, me, slave);
#endif
	if (sem_post(&(shData->queue))<0){
		perror("Post queue");
	}

	sem_post(&(shData->lock_data));

    return slave;
}

void GetFromMaster(char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Slave waiting in semaphore\n", node, me);
#endif
    sem_wait(&(msg4slave[me]));
	memcpy(info, &(threads[me]), size);

#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Slave received %d from master\n",node,  me, *(int*)info);
#endif
}

void GetFromMasterNonBlocking(char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Reading from Master\n", node, me);
#endif
		memcpy(info, &(threads[me]), size);
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Slave received %d from master\n",node,  me, *(int*)info);
#endif
}

void SendToMaster(char *info,int size){

#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Send to Master\n", node, me);
#endif

	//check if there is space in the queue
	sem_wait(&(shData->queue));

    sem_wait(&(shData->lock_data));

	memcpy(&(msgs[shData->last].data), info, size);

	msgs[shData->last].proc=me;

	shData->last = (shData->last+1)%size_queue;

//	if (shData->last==shData->first) fprintf(stderr,"%d:%d WARNING SH.MEM msg queue overflow\n", node, me);

    sem_post(&(shData->lock_data));
    if (sem_post(&(shData->msg4master))<0){
		perror("Post to master");
	}

}

void GetFromSlave(int rank,char *info,int size){
}

void WriteToSlave(int rank,char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Send to Slave %d : %d\n", node, me, rank, *(int*)info);
#endif

	memcpy(&(threads[rank]), info, size);
}

void SendToSlave(int rank,char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"DLB DEBUG: (%d:%d) Send to Slave %d : %d\n", node, me, rank, *(int*)info);
#endif

	memcpy(&(threads[rank]), info, size);

    if(sem_post(&(msg4slave[rank]))<0){
		perror("Post to slave");
	}
}

void comm_close(){
	if (shmctl(shmid, IPC_RMID, NULL)<0)
		perror("Removing Shared Memory");	
}
