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
#include "config.h"
#endif

#include <comm.h>
#include "support/tracing.h"
#include "support/globals.h"

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
    fprintf(stderr,"DLB DEBUG: (%d:%d) - %d LoadCommonConfig\n", node,me,  getpid());
#endif
	procs=num_procs;
	me=meId;
	node=nodeId;
	defaultCPUS=defCPUS;
	greedy=is_greedy;

	int k;
	key_t key;
    	int sm_size;
    	char * shm;


	sm_size= sizeof(struct shdata);
	//       idle cpus      


	if (me==0){

		k=getpid();
		key=k;
       
	
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

                shdata = (struct shdata *)shm;
	
		
	
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) setting values to the shared mem\n", node, me);
#endif
		/* idleCPUS */
                shdata->idleCpus = 0;
		add_event(IDLE_CPUS_EVENT, 0);

#ifdef HAVE_MPI
		PMPI_Bcast ( &k, 1, MPI_INTEGER, 0, _mpi_comm_node);
#endif

#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Finished setting values to the shared mem\n", node, me);
#endif

	}else{
#ifdef debugSharedMem 
    	fprintf(stderr,"DLB DEBUG: (%d:%d) Slave Comm - associating to shared mem\n", node, me);
#endif
#ifdef HAVE_MPI
		PMPI_Bcast ( &k, 1, MPI_INTEGER, 0, _mpi_comm_node);
#else
                k=0;
#endif
		key=k;
		
		shmid = shmget(key, sm_size, 0666);
	
		while (shmid<0 && errno==ENOENT){
			shmid = shmget(key, sm_size, 0666);
		}
		if (shmid < 0) {
			perror("shmget slave");
			exit(1);
		}
	
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Slave Comm - associated to shared mem\n", node, me);
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
		perror("DLB ERROR: Removing Shared Memory");	
}

int releaseCpus(int cpus){
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Releasing CPUS...\n", node, me);
#endif
	__sync_fetch_and_add (&(shdata->idleCpus), cpus); 
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) DONE Releasing CPUS (idle %d) \n", node, me, shdata->idleCpus);
#endif
  return 0;
}

/*
Returns de number of cpus
that are assigned
*/
int acquireCpus(int current_cpus){
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Acquiring CPUS...\n", node, me);
#endif
  int cpus = defaultCPUS-current_cpus;

//I don't care if there aren't enough cpus

	if((__sync_sub_and_fetch (&(shdata->idleCpus), cpus)>0) && greedy){ 
		cpus+=__sync_val_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, 0);
    	}
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Using %d CPUS... %d Idle \n", node, me, cpus, shdata->idleCpus);
#endif

  return cpus+current_cpus;
}

/*
Returns de number of cpus
that are assigned
*/
int checkIdleCpus(int myCpus){
#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Checking idle CPUS... %d\n", node, me, shdata->idleCpus);
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
        if(aux>defaultCPUS) aux=defaultCPUS;

	if(__sync_bool_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, shdata->idleCpus-aux)){
		myCpus+=aux;
	}
    }
	add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugSharedMem 
		fprintf(stderr,"DLB DEBUG: (%d:%d) Using %d CPUS... %d Idle \n", node, me, myCpus, shdata->idleCpus);
#endif
  return myCpus;
}
