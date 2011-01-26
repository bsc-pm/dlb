#include <comm.h>
#include <LB_arch/common.h>
#include <LB_MPI/tracing.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <mpi.h>

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))

int procs;
int me;
int node;

int defaultCPUS;
int greedy;
//pointers to the shared memory structures
struct shdata {
   int   idleCpus;
};

struct shdata *shdata;
int shmid;

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
	key_t key;
    	int sm_size;
    	char * shm;

	if ((k=getenv("SLURM_JOBID"))==NULL){
		fprintf(stdout,"INFO: SLURM_JOBID not found using parent pid(%d) as shared Memory name\n", getppid());
		key=getppid();
	}else{
		key=atol(k);
	}

	sm_size= sizeof(struct shdata);
	//       idle cpus      

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

                shdata = (struct shdata *)shm;
	
		
	
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d setting values to the shared mem\n", node, me);
#endif
		/* idleCPUS */
                shdata->idleCpus = 0;
		add_event(IDLE_CPUS_EVENT, 0);

		PMPI_Barrier(MPI_COMM_WORLD);
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Finished setting values to the shared mem\n", node, me);
#endif

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
	
		shdata = (struct shdata *)shm;
	}
}

void finalize_comm(){
	if (shmctl(shmid, IPC_RMID, NULL)<0)
		perror("Removing Shared Memory");	
}

int releaseCpus(int cpus){
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Releasing CPUS...\n", node, me);
#endif
	__sync_fetch_and_add (&(shdata->idleCpus), cpus); 
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d DONE Releasing CPUS (idle %d) \n", node, me, shdata->idleCpus);
#endif
  return 0;
}

/*
Returns de number of cpus
that are assigned
*/
int acquireCpus(int current_cpus){
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Acquiring CPUS...\n", node, me);
#endif
  int cpus = defaultCPUS-current_cpus;

//I don't care if there aren't enough cpus

	if((__sync_sub_and_fetch (&(shdata->idleCpus), cpus)>0) && greedy){ 
		cpus+=__sync_val_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, 0);
    	}
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Using %d CPUS... %d Idle \n", node, me, cpus, shdata->idleCpus);
#endif

  return cpus+current_cpus;
}

/*
Returns de number of cpus
that are assigned
*/
int checkIdleCpus(int myCpus){
#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Checking idle CPUS... %d\n", node, me, shdata->idleCpus);
#endif
  int cpus;
  int aux;
//WARNING//
    //if more CPUS than the availables are used release some
    if ((shdata->idleCpus < 0) && (myCpus>defaultCPUS) ){
	aux=shdata->idleCpus;
	cpus=MIN(abs(aux), myCpus-defaultCPUS);
	if(__sync_bool_compare_and_swap(&(shdata->idleCpus), aux, aux+cpus)){
		myCpus-=cpus;
	}

    //if there are idle CPUS use them
    }else if( shdata->idleCpus > 0){
	aux=shdata->idleCpus;
	if(__sync_bool_compare_and_swap(&(shdata->idleCpus), aux, 0)){
		myCpus+=aux;
	}
    }
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"%d:%d Using %d CPUS... %d Idle \n", node, me, myCpus, shdata->idleCpus);
#endif
  return myCpus;
}
