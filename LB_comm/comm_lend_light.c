#include <comm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <mpi.h>

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))

int procs;
int me;
int node;

int defaultCPUS;
int greedy;

//pointers to the shared memory structures
int* idleCpus;
sem_t* sem;


void ConfigShMem(int num_procs, int meId, int nodeId, int defCPUS, int is_greedy){
#ifdef debugSharedMem 
    fprintf(stderr,"%d:%d - %d LoadCommonConfig\n", node,me,  getpid());
#endif
	procs=num_procs;
	me=meId;
	node=nodeId;
	defaultCPUS=defCPUS;
	greedy=is_greedy;

	char *k;
	int shmid, i;
    key_t key;
    int sm_size;
    char * shm;

	if ((k=getenv("SLURM_JOBID"))==NULL){
		fprintf(stdout,"INFO: SLURM_JOBID not found using constant shared Memory name\n");
		key=42424242;
	}else{
		key=atol(k);
	}

	sm_size= sizeof(int) + sizeof(sem_t);
	//       idle cpus       semaphore

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
	
		
	
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d setting values to the shared mem\n", node, me);
#endif
		/* idleCPUS */
		idleCpus=(int*)shm;
		*idleCpus=0;

		//The sempahore  to acces the shared data
		sem = (sem_t*)((char*)idleCpus + sizeof(int)); 
		if (sem_init(sem, 1, 1)<0){
			perror("sem_init");
			exit(1);
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
	
		// idle CPUs
		idleCpus=(int*)shm;

		//The sempahore  to acces the shared data
		sem = (sem_t*)((char*)idleCpus + sizeof(int)); 
	}
}

int releaseCpus(int cpus){
  if (sem_wait(sem)<0){
    perror("Wait sem");
    return -1;
  }

    (*idleCpus)+=cpus;

  if (sem_post(sem)<0){
    perror("Post sem");
    return -1;
  }
  return 0;
}

/*
Returns de number of cpus
that are assigned
*/
int acquireCpus(){
  int cpus = defaultCPUS;
  if (sem_wait(sem)<0){
    perror("Wait sem");
    return -1;
  }

//I don't care if there are enough cpus
    (*idleCpus)-=defaultCPUS;

    if ((greedy)&&((*idleCpus)>0)){
	cpus+=(*idleCpus);
	(*idleCpus)=0;
    }

  if (sem_post(sem)<0){
    perror("Post sem");
    return -1;
  }
  return cpus;
}

/*
Returns de number of cpus
that are assigned
*/
int checkIdleCpus(int myCpus){
  int cpus;

  if (sem_wait(sem)<0){
    perror("Wait sem");
    return -1;
  }
    //if more CPUS than the availables are used release some
    if ( ((*idleCpus) < 0) && (myCpus>defaultCPUS) ){
      cpus=MIN(abs(*idleCpus), myCpus-defaultCPUS);
      (*idleCpus)+=cpus;
      myCpus-=cpus;

    //if there are idle CPUS use them
    }else if((*idleCpus) > 0){
      myCpus+=(*idleCpus);
      (*idleCpus)=0;
    }

  if (sem_post(sem)<0){
    perror("Post sem");
    return -1;
  }
  return myCpus;
}
