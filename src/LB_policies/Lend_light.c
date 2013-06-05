#include <Lend_light.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_lend_light.h>
#include "support/globals.h"
#include "support/mask_utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/resource.h>

int me, node, procs;
int greedy;
int default_cpus;
int myCPUS;
int block;

/* Blocking modes */
#define _1CPU 0 // MPI not set to blocking, leave a cpu while in a MPI blockin call
#define MPI_BLOCKING 1 //MPI set to blocking mode
#define PRIO 2 // MPI not set to blocking, decrease priority when in a blocking call

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Lend_light Init\n", nodeId, meId);
#endif
	char* policy_greedy;
	char* blocking;
	me = meId;
	node = nodeId;
	procs = num_procs;
	default_cpus = _default_nthreads;
	
	setThreads_Lend_light(default_cpus);
//	myCPUS=default_cpus;

#ifdef debugBasicInfo
	    if (me==0 && node==0){
	      fprintf(stdout, "DLB: Default cpus per process: %d\n", default_cpus);
	    }
#endif

	char* bind;
	if ((bind=getenv("LB_BIND"))!=NULL){
	    	if(strcasecmp(bind, "YES")==0){
	      		fprintf(stdout, "DLB: Binding of threads to cpus enabled\n");
			if (css_set_num_threads){ //If we are running with openMP we must bind the slave threads
				bind_master(meId, nodeId);
			}else{
	      			fprintf(stderr, "DLB ERROR: Binding of threads only enabled for SMPSs\n");
			}
		}
	}

	greedy=0;
	if ((policy_greedy=getenv("LB_GREEDY"))!=NULL){
	    if(strcasecmp(policy_greedy, "YES")==0){
	    greedy=1;
#ifdef debugBasicInfo
	    if (me==0 && node==0){
	      fprintf(stdout, "DLB: Policy mode GREEDY\n");
	    }
#endif
	    }
	}

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
	ConfigShMem(procs, me, node, default_cpus, greedy);

      if ( _aggressive_init ) {
         setThreads_Lend_light( mu_get_system_size() );
         setThreads_Lend_light( _default_nthreads );
      }
}

void Lend_light_Finish(void){
	if (me==0) finalize_comm();
}

void Lend_light_InitIteration(void){}

void Lend_light_FinishIteration(void){}

void Lend_light_IntoCommunication(void){}

void Lend_light_OutOfCommunication(void){}

void Lend_light_IntoBlockingCall(void){

	int prio;
	if (block==_1CPU){
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS-1);
#endif
		releaseCpus(myCPUS-1);
		setThreads_Lend_light(1);
	}else{
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, myCPUS);
#endif
		releaseCpus(myCPUS);
		setThreads_Lend_light(0);
		if (block==PRIO){
			fprintf(stderr,"Current priority %d\n", getpriority(PRIO_PROCESS, 0));
			if((prio=nice(19))<0) perror("Error setting priority 'nice'");
			else fprintf(stderr,"%d:%d - Priority set to %d\n", node, me, prio);
		}
	}
}

void Lend_light_OutOfBlockingCall(void){
	int prio;

	if (block==PRIO){
		fprintf(stderr,"Current priority %d\n", getpriority(PRIO_PROCESS, 0));

		if((prio=nice(-1))<0) perror("Error setting priority 'nice'");
		else fprintf(stderr,"%d:%d - Priority set to %d\n", node, me, prio);
	}

	int cpus=acquireCpus(myCPUS);
	setThreads_Lend_light(cpus);
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", node, me, cpus);
#endif

}

/******* Auxiliar Functions Lend_light Balancing Policy ********/

void Lend_light_updateresources(){
	int cpus = checkIdleCpus(myCPUS);
	if (myCPUS!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, cpus);
#endif
		 setThreads_Lend_light(cpus);
	}
}

void setThreads_Lend_light(int numThreads){

	if (myCPUS!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, numThreads);
#endif
		update_threads(numThreads);
		myCPUS=numThreads;
	}
}
