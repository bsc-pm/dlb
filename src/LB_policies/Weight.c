#include <Weight.h>
#include <LB_arch/arch.h>
#include <support/utils.h>

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <LB_numThreads/numThreads.h>

int me, node, procs;
int finished;
int threads2use, threadsUsed;
static sem_t sem_localMetrics;
ProcMetrics localMetrics;

/******* Main Functions Weight Balancing Policy ********/

void Weight_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Weight Init\n", nodeId, meId);
#endif
	me = meId;
	node = nodeId;
	procs = num_procs;

	//Create auxiliar threads
	createThreads_Weight();	
}

void Weight_Finish(void){}

void Weight_InitIteration(void){}

void Weight_FinishIteration(double cpuSecs, double MPISecs){
	
	ProcMetrics pm;
	pm.secsComp=cpuSecs;
	pm.secsMPI=MPISecs;
	pm.cpus=threadsUsed;
	SendLocalMetrics(pm);

}

void Weight_IntoCommunication(void){}

void Weight_OutOfCommunication(void){
	Weight_updateresources();
}

void Weight_IntoBlockingCall(double cpuSecs, double MPISecs){}

void Weight_OutOfBlockingCall(void){}

/******* Auxiliar Functions Weight Balancing Policy ********/

/* Creates auxiliar threads */
void createThreads_Weight(){
	pthread_t t;
	finished=0;

#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Creating Threads\n", node, me);
#endif

	threadsUsed=0;
	threads2use=CPUS_NODE/procs;

	//The local thread won't communicate by sockets
	LoadCommConfig(procs, me, node);

	if (me==0){ 
		if (pthread_create(&t,NULL,masterThread_Weight,NULL)>0){
			perror("DLB PANIC: createThreads_Weight:Error in pthread_create master\n");
			exit(1);
		}
	}

	if(pthread_create(&t,NULL,slaveThread_Weight,NULL)>0){
		perror("DLB PANIC: createThreads_Weight:Error in pthread_create slave\n");
		exit(1);
	}
	
	Weight_updateresources();
}

/******* Master Thread Functions ********/
void* masterThread_Weight(void* arg){

	int cpus[procs];
	int i;

#ifdef debugConfig
	fprintf(stderr,"DLB DEBUG: (%d:%d) - Creating Master thread\n", node, me);
#endif	

	StartMasterComm();

	ProcMetrics currMetrics[procs];

	if (sem_init(&sem_localMetrics,0,0)<0){
		perror("DLB PANIC: Initializing metrics semaphore\n");
		exit(1);
	}

	//We start with equidistribution
	for (i=0; i<procs; i++) cpus[i]=CPUS_NODE/procs;

//	applyNewDistribution_Weight(cpus);

	while(!finished){
		GetMetrics(currMetrics);
		CalculateNewDistribution_Weight(currMetrics, cpus);
		applyNewDistribution_Weight(cpus);
	}
        return NULL;
}

void GetMetrics(ProcMetrics metrics[]){
	int i, slave;
	ProcMetrics metr;

	/*getLocaMetrics(&metr);
	metrics[0]=metr;*/

	for (i=0; i<procs; i++){
		slave=GetFromAnySlave((char *) &metr,sizeof(ProcMetrics));
		metrics[slave]=metr;
	}
}

void getLocaMetrics(ProcMetrics *metr){
	sem_wait(&sem_localMetrics);
	*metr=localMetrics;
}

void SendLocalMetrics(ProcMetrics LM)
{
	localMetrics = LM;
	sem_post(&sem_localMetrics);
}

void CalculateNewDistribution_Weight(ProcMetrics LM[], int cpus[]){
	double total_time=0;
	int i, cpus_alloc, total_cpus=0;
	double weights[procs];

	//Calculate total runtime
	for (i=0; i<procs; i++){
		total_time+=(LM[i].secsComp * LM[i].cpus);
	}

	//Calculate weigth per process
	for (i=0; i<procs; i++){
		weights[i]=(double)(LM[i].secsComp * (double)LM[i].cpus *(double)100/(double)total_time);
#ifdef debugLoads
	fprintf(stderr,"DLB DEBUG: [Process %d] Comp. time: %f.4 - cpus: %d - Load: %f.4\n", i, LM[i].secsComp, LM[i].cpus, weights[i]);
#endif
	}

#ifdef debugDistribution
	fprintf(stderr,"DLB DEBUG: New Distribution: ");
#endif
	//Calculate new distribution
	for (i=0; i<procs; i++){
		
		if (weights[i]>=(double)100){	
			cpus[i]=LM[i].cpus;
		}else{ 
			cpus_alloc=my_round((weights[i]*(double)CPUS_NODE)/(double)100);

			if (cpus_alloc>(CPUS_NODE-(procs-1))) cpus[i]=CPUS_NODE-(procs-1);
			else if (cpus_alloc<1) cpus[i]=1;
			else cpus[i]=cpus_alloc; 	
		}
		
		#ifdef debugDistribution
			fprintf(stderr,"[%d]", cpus[i]);
		#endif
		total_cpus+=cpus[i];
	}
#ifdef debugDistribution
	fprintf(stderr,"\n");
#endif
	if (total_cpus>CPUS_NODE){
		fprintf(stderr,"DLB WARNING: Using more cpus than the ones available in the node (%d>%d)\n", total_cpus, CPUS_NODE);
	}else if(total_cpus<CPUS_NODE){
		fprintf(stderr,"DLB WARNING: Using less cpus than the ones available in the node (%d<%d)\n", total_cpus, CPUS_NODE);
	}
}

void applyNewDistribution_Weight(int cpus[]){
	int i;
	//threads2use = cpus[0];
	for (i=0; i<procs; i++){
		SendToSlave(i, (char*)&cpus[i], sizeof(int));
	}
}

/******* Slave Thread Functions ********/
void* slaveThread_Weight(void* arg){
	int cpus;
	ProcMetrics metr;

	if (sem_init(&sem_localMetrics,0,0)<0){
		perror("DLB PANIC: Initializing metrics semaphore\n");
		exit(1);
	}

#ifdef debugConfig
	fprintf(stderr,"DLB DEBUG: (%d:%d) - Creating Slave thread\n", node, me);
#endif	
	StartSlaveComm();

	while(!finished){
		getLocaMetrics(&metr);
		SendToMaster((char*)&metr, sizeof(ProcMetrics));
		GetFromMaster((char*)&cpus,sizeof(int));
		threads2use=cpus;
	}
        return NULL;
}

void Weight_updateresources(){
	if (threadsUsed!=threads2use){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, threads2use);
#endif
		update_threads(threads2use);
		threadsUsed=threads2use;
	}
}
