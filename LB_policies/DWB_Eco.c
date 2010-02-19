#include <DWB_Eco.h>
#include <LB_arch/arch.h>
#include <LB_arch/common.h>

#include <semaphore.h>
#include <stdio.h>


int me, node, procs;
int finished;
int threads2use, threadsUsed;
static sem_t sem_localMetrics;
ProcMetrics localMetrics;

/******* Main Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "%d:%d - DWB_Eco Init\n", nodeId, meId);
#endif
	me = meId;
	node = nodeId;
	procs = num_procs;
	threadsUsed=CPUS_NODE/procs;
}

void DWB_Eco_Finish(void){}

void DWB_Eco_InitIteration(void){}

void DWB_Eco_FinishIteration(double cpuSecs, double MPISecs){
	
	double n;
	int x;
/*	ProcMetrics pm;
	pm.secsComp=cpuSecs;
	pm.secsMPI=MPISecs;
	pm.cpus=threadsUsed;*/
	
	n = ((float)threadsUsed*cpuSecs)/(cpuSecs+MPISecs);
	x = (n+1);
	if(x<1) x=1;
	if(x>(CPUS_NODE-0.5))x=CPUS_NODE;
	update_threads(x);
	if(threadsUsed!=x) fprintf(stderr, "%d:%d n=%f x=%d\n", node, me, n, x);
	threadsUsed=x;
	
	
	
	//SendLocalMetrics_DWB_Eco(pm);

}

void DWB_Eco_IntoCommunication(void){}

void DWB_Eco_OutOfCommunication(void){
	//updateresources();
}

void DWB_Eco_IntoBlockingCall(double cpuSecs, double MPISecs){}

void DWB_Eco_OutOfBlockingCall(void){}

/******* Auxiliar Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_updateresources(){
	/*if (threadsUsed!=threads2use){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, threads2use);
#endif
		update_threads(threads2use);
		threadsUsed=threads2use;
	}*/
}

