#include <Lewi_map.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_map.h>
#include "support/globals.h"
#include "support/tracing.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/resource.h>

int me, node, procs;
int default_cpus;
int myCPUS=0;
int block;
int *my_cpus;

/* Blocking modes */
#define _1CPU 0 // MPI not set to blocking, leave a cpu while in a MPI blockin call
#define MPI_BLOCKING 1 //MPI set to blocking mode

/******* Main Functions Map Balancing Policy ********/

void Map_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Map Init\n", nodeId, meId);
#endif
	char* blocking;
	my_cpus= malloc(sizeof(int)*CPUS_NODE);
	me = meId;
	node = nodeId;
	procs = num_procs;
	default_cpus = _default_nthreads;
	
#ifdef debugBasicInfo
	    if (me==0 && node==0){
	      fprintf(stdout, "DLB: Default cpus per process: %d\n", default_cpus);
	    }
#endif
	

//Setting blocking mode
	block=_1CPU;

	if ((blocking=getenv("LB_LEND_MODE"))!=NULL){
		if (strcasecmp(blocking, "BLOCK")==0){
			block=MPI_BLOCKING;
#ifdef debugBasicInfo 
			fprintf(stderr, "DLB (%d:%d) - LEND mode set to BLOCKING. I will lend all the resources when in an MPI call\n", node, me);
#endif	
		}
	}
#ifdef debugBasicInfo 
	if (block==_1CPU)
		fprintf(stderr, "DLB: (%d:%d) - LEND mode set to 1CPU. I will leave a cpu per MPI process when in an MPI call\n", node, me);
#endif	

	//Initialize shared memory
	ConfigShMem_Map(procs, me, node, default_cpus, my_cpus);
//fprintf(stderr, "DLB: (%d:%d) - setting threads %d (%d, %d)\n", node, me, default_cpus, my_cpus[0], my_cpus[23]);
	setThreads_Map(default_cpus, 1, my_cpus);

}

void Map_Finish(void){
	free(my_cpus);
	if (me==0) finalize_comm_Map();
}

void Map_InitIteration(){}

void Map_FinishIteration(void){}

void Map_IntoCommunication(void){}

void Map_OutOfCommunication(void){}

void Map_IntoBlockingCall(void){

	int res;
	if (block==_1CPU){
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS-1);
#endif
		res=myCPUS;
		setThreads_Map(1, 3, my_cpus);
		res=releaseCpus_Map(res-1, my_cpus);
	}else{
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS);
#endif
		res=myCPUS;
		setThreads_Map(0, 3, my_cpus);
		res=releaseCpus_Map(res, my_cpus);
	}
}

void Map_OutOfBlockingCall(void){
   //int prio;

	int cpus=acquireCpus_Map(myCPUS, my_cpus);
	setThreads_Map(cpus, 1, my_cpus);
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", node, me, cpus);
#endif

}

/******* Auxiliar Functions Map Balancing Policy ********/

void Map_updateresources(int max_cpus){
	int action;
	int cpus = checkIdleCpus_Map(myCPUS, max_cpus, my_cpus);
	if (myCPUS!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, cpus);
#endif
		if (cpus>myCPUS) action=1;
		if (cpus<myCPUS) action=2;
		 setThreads_Map(abs(cpus), action, my_cpus);
	}
}

void setThreads_Map(int numThreads, int action, int* cpus){
//fprintf(stderr, "DLB: (%d:%d) - setting threads inside %d (%d -> %d)\n", node, me, action, myCPUS, numThreads);

	if (myCPUS!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - I have %d cpus I'm going to use %d cpus\n", node, me, myCPUS, numThreads);
#endif
		add_event(THREADS_USED_EVENT, numThreads);
		int num_cpus;

		if (action==1) num_cpus=numThreads-myCPUS;
		if (action==2) num_cpus=myCPUS-numThreads;
		if (action==3) num_cpus=myCPUS-numThreads;
		if ((numThreads==0)||(myCPUS==0)){ 
			num_cpus--; //We always leave a cpu to smpss
			cpus=&cpus[1]; 
		}
		

                update_cpus(action, num_cpus, cpus);
//fprintf(stderr, "(%d:%d) ******************** CPUS %d --> %d\n", node, me, myCPUS, numThreads);
		myCPUS=numThreads;
	}
}
