#include <Lend_light.h>
#include <LB_arch/arch.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_lend_light.h>
#include <LB_MPI/tracing.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

int me, node, procs;
int greedy;
int default_cpus;
int myCPUS;


/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "%d:%d - Lend_light Init\n", nodeId, meId);
#endif
	char* policy_greedy;

	me = meId;
	node = nodeId;
	procs = num_procs;
	default_cpus=CPUS_NODE/procs;
	
	setThreads_Lend_light(default_cpus);
	myCPUS=default_cpus;

#ifdef debugBasicInfo
	    if (me==0 && node==0){
	      fprintf(stdout, "Default cpus per process: %d\n", default_cpus);
	    }
#endif

	greedy=0;
	if ((policy_greedy=getenv("LB_GREEDY"))!=NULL){
	    if(strcasecmp(policy_greedy, "YES")==0){
	    greedy=1;
#ifdef debugBasicInfo
	    if (me==0 && node==0){
	      fprintf(stdout, "Policy mode GREEDY\n");
	    }
#endif
	    }
	}

	//Initialize shared memory
	ConfigShMem(procs, me, node, default_cpus, greedy);

}

void Lend_light_Finish(void){
	if (me==0) finalize_comm();
}

void Lend_light_InitIteration(){}

void Lend_light_FinishIteration(double cpuSecs, double MPISecs){}

void Lend_light_IntoCommunication(void){}

void Lend_light_OutOfCommunication(void){}

void Lend_light_IntoBlockingCall(double cpuSecs, double MPISecs){
	
#ifdef debugLend
	fprintf(stderr, "%d:%d - LENDING %d cpus\n", node, me, myCPUS);
#endif

	releaseCpus(myCPUS);
	setThreads_Lend_light(0);
	
}

void Lend_light_OutOfBlockingCall(void){

	int cpus=acquireCpus();
	setThreads_Lend_light(cpus);
#ifdef debugLend
	fprintf(stderr, "%d:%d - ACQUIRING %d cpus\n", node, me, cpus);
#endif

}

/******* Auxiliar Functions Lend_light Balancing Policy ********/

void Lend_light_updateresources(){
	int cpus = checkIdleCpus(myCPUS);

	if (myCPUS!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, cpus);
#endif
		 setThreads_Lend_light(cpus);
	}
}

void setThreads_Lend_light(int numThreads){

	if (myCPUS!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, numThreads);
#endif
		update_threads(numThreads);
		myCPUS=numThreads;
	}
}
