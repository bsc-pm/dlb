#include <comm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <mpi.h>

int procs;
int me;
int node;

int size_queue;

//pointers to the shared memory structures
sharedData* shData;
msg * msgs;
sem_t * msg4slave;
int * threads;

void LoadCommConfig(int num_procs, int meId, int nodeId){
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d - %d LoadCommonConfig\n", node,me,  getpid());
#endif
	procs=num_procs;
	me=meId;
	node=nodeId;

	char *k;
	int shmid, i;
    key_t key;
    int sm_size;
    char * shm;
	size_queue=procs*100;

	if ((k=getenv("SLURM_JOBID"))==NULL){
		fprintf(stdout,"INFO: SLURM_JOBID not found using constant shared Memory name\n");
		key=42424242;
	}else{
		key=atol(k);
	}

	sm_size= sizeof(sharedData) + (sizeof(msg)*size_queue) + (sizeof(sem_t)*procs)+ (sizeof(int)*procs);
	//         the struct         the queue of messages       the semaphores         the threads    

	if (me==0){
       
	
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Start Master Comm - creating shared mem \n", node, me);
#endif
	
		if ((shmid = shmget(key, sm_size, IPC_EXCL | IPC_CREAT | 0666)) < 0) {
			perror("shmget Master");
			exit(1);
		}
	
		if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
			perror("shmat Master");
			exit(1);
		}
	
		shData=(sharedData*)shm;
	
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d setting values to the shared mem\n", node, me);
#endif

		//The top and the tail of the queue
		shData->first=0;
		shData->last=0;

		//The semaphore to warn the master of a new message
		if (sem_init(&(shData->msg4master), 1, 0)<0){
			perror("sem_init msg4master");
		exit(1);
		}

		//The semaphore to have exclusive acces to the data
		if (sem_init(&(shData->lock_data), 1, 1)<0){
			perror("sem_init lock_data");
			exit(1);
		}

		//The semaphore to have space in the queue
		if (sem_init(&(shData->queue), 1, (size_queue-1))<0){
			perror("sem_init lock_data");
			exit(1);
		}

		//The queue of messages for the master
		msgs = (msg*)((char*)shData + sizeof(sharedData)); 


		//The array of sempahores to warn the slaves of a change in the number of threads
		msg4slave =  (sem_t*)((char*)msgs + (sizeof(msg)*size_queue)); 
		for (i=0; i<procs; i++){
			if (sem_init(&(msg4slave[i]), 1, 0)<0){
				perror("sem_init msg4slave");
				exit(1);
			}
		}	

		//The array with the threads to be used by each slave
		threads = (int*)((char*)msg4slave + (sizeof(sem_t)*procs));
		for (i=0; i<procs; i++){
			threads[i]=0;
		}

		PMPI_Barrier(MPI_COMM_WORLD);

	}else{
#ifdef debugSharedMem 
    	fprintf(stderr,"%d:%d Slave Comm - associating to shared mem\n", node, me);
#endif

		PMPI_Barrier(MPI_COMM_WORLD);
	
		
		shmid = shmget(key, sm_size, 0666);
	
		while (shmid<0 && errno==ENOENT){
			shmid = shmget(key, sm_size, 0666);
		}
		if (shmid < 0) {
			perror("shmget slave");
			exit(1);
		}
	
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Slave Comm - associated to shared mem\n", node, me);
#endif
		if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
			perror("shmat slave");
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
	fprintf(stderr,"%d:%d - ", node, me);
	for (i=0; i<size_queue; i++){
		fprintf(stderr,"[%d,%d]", msgs[i].proc, msgs[i].data.action);
	}
	fprintf(stderr,"\n");
	
	fprintf(stderr,"%d:%d - Num Threads", node, me);
	for (i=0; i<procs; i++){
		fprintf(stderr,"[%d]", threads[i]);
	}
	fprintf(stderr,"\n");
}

int GetFromAnySlave(char *info,int size){
	int slave;
	
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Master waiting in semaphore\n", node, me);
#endif
    sem_wait(&(shData->msg4master));
    sem_wait(&(shData->lock_data));

	slave=msgs[shData->first].proc;
	memcpy(info, &(msgs[shData->first].data), size);

	shData->first= (shData->first+1)%size_queue;

#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Master received from %d\n", node, me, slave);
#endif
	if (sem_post(&(shData->queue))<0){
		perror("Post queue");
	}

	sem_post(&(shData->lock_data));

    return slave;
}

void GetFromMaster(char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Slave waiting in semaphore\n", node, me);
#endif
    sem_wait(&(msg4slave[me]));
	memcpy(info, &(threads[me]), size);

#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Slave received %d from master\n",node,  me, *(int*)info);
#endif
}

void GetFromMasterNonBlocking(char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Reading from Master\n", node, me);
#endif
		memcpy(info, &(threads[me]), size);
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Slave received %d from master\n",node,  me, *(int*)info);
#endif
}

void SendToMaster(char *info,int size){

#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Send to Master\n", node, me);
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
    fprintf(stderr,"%d:%d Send to Slave %d : %d\n", node, me, rank, *(int*)info);
#endif

	memcpy(&(threads[rank]), info, size);
}

void SendToSlave(int rank,char *info,int size){
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d Send to Slave %d : %d\n", node, me, rank, *(int*)info);
#endif

	memcpy(&(threads[rank]), info, size);

    if(sem_post(&(msg4slave[rank]))<0){
		perror("Post to slave");
	}
}

void comm_close(){
	//shmdt(shData);
}
