#include <Lend_simple.h>
#include <LB_numThreads/numThreads.h>
#include "support/globals.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int me, node, procs;
int finished;
int threads2use, threadsUsed;
int idleCpus;

/******* Main Functions Lend Balancing Policy ********/

void Lend_simple_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Lend Init\n", nodeId, meId);
#endif
	me = meId;
	node = nodeId;
	procs = num_procs;

	//Create auxiliar threads
	createThreads_Lend_simple();	
}

void Lend_simple_Finish(void){
	finished=1;
	comm_close();
}

void Lend_simple_InitIteration(){}

void Lend_simple_FinishIteration(double cpuSecs, double MPISecs){}

void Lend_simple_IntoCommunication(void){
}

void Lend_simple_OutOfCommunication(void){
	Lend_simple_updateresources();
}

void Lend_simple_IntoBlockingCall(double cpuSecs, double MPISecs){
	ProcMetrics info;
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", node, me, threadsUsed);
#endif
	info.action = LEND_CPUS;
	info.cpus=threadsUsed;

	threads2use=0;
	Lend_simple_updateresources();
	SendToMaster((char *)&info,sizeof(info));
	threads2use=0;
	Lend_simple_updateresources();
}

void Lend_simple_OutOfBlockingCall(void){
	ProcMetrics info;
	info.action = ACQUIRE_CPUS;
	info.cpus=threadsUsed;

	threads2use=_default_nthreads;
	Lend_simple_updateresources();

	SendToMaster((char *)&info,sizeof(info));
	threads2use=_default_nthreads;
	Lend_simple_updateresources();
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", node, me, threadsUsed);
#endif
}

/******* Auxiliar Functions Lend Balancing Policy ********/

/* Creates auxiliar threads */
void createThreads_Lend_simple(){
	pthread_t t;
	finished=0;

	threads2use=_default_nthreads;

#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Creating Threads\n", node, me);
#endif
	LoadCommConfig(procs, me, node);

	if (me==0){ 
		if (pthread_create(&t,NULL,masterThread_Lend_simple,NULL)>0){
			perror("DLB PANIC: createThreads:Error in pthread_create master\n");
			exit(1);
		}
	}

	if (pthread_create(&t,NULL,slaveThread_Lend_simple,NULL)>0){
		perror("DLB PANIC: createThreads:Error in pthread_create slave\n");
		exit(1);
	}
}

/******* Master Thread Functions ********/
void* masterThread_Lend_simple(void* arg){

	int cpus[procs];
	int i, changed;
	//int info;
	ProcMetrics info;

#ifdef debugConfig
	fprintf(stderr,"DLB DEBUG: (%d:%d) - Creating Master thread\n", node, me);
#endif	

	StartMasterComm();
	idleCpus=0;
	

	//We start with equidistribution
	for (i=0; i<procs; i++) cpus[i]=_default_nthreads;

	applyNewDistribution_Lend_simple(cpus, procs+1);

	while(!finished){
		i = GetFromAnySlave((char*)&info, sizeof(info));

		changed=CalculateNewDistribution_Lend_simple(cpus, i, info);
		//applyNewDistribution(cpus, i);

#ifdef debugDistribution
			fprintf(stderr,"DLB DEBUG: (%d:%d) - New Distribution: ", node, me);
			for (i=0; i<procs; i++) fprintf(stderr,"[%d]", cpus[i]);
			fprintf(stderr,"\n");
#endif
		if (changed>=0) {
			SendToSlave(changed,(char*) &cpus[changed] , sizeof(int));
			
		}
	}
        return NULL;
}

void applyNewDistribution_Lend_simple(int * cpus, int except){
	int i;
#ifdef debugDistribution
	fprintf(stderr,"DLB DEBUG: (%d:%d) - New Distribution: ", node, me);
	for (i=0; i<procs; i++) fprintf(stderr,"[%d]", cpus[i]);
	fprintf(stderr,"\n");
#endif
	for (i=0; i<procs; i++){
		if(i!=except){
			SendToSlave(i,(char*) &cpus[i] , sizeof(int));
		}
	}
}

/* Pasa las cpus liberadas al siguiente en la lista */
int CalculateNewDistribution_Lend_simple(int* cpus, int process, ProcMetrics info){
	int i;
	int lended;
	int changed = -1;

	if (info.action == LEND_CPUS){
		if(cpus[process]==0){
			fprintf(stderr,"DLB WARNING: (%d:%d) - Process %d trying to Lend cpus when does not have any\n", node, me, process);
		}else{
			lended=cpus[process];


			i = (process + 1)%procs;
			while ( (i!=process) && (cpus[i]==0) ){
				i = (i + 1)%procs;
			}
			if (i!=process){
				cpus[i]+=lended;
				changed=i;
			}else{
				idleCpus+=lended;
			}
			cpus[process]=0;
		}
	}else{
		if(cpus[process]!=0){
			fprintf(stderr,"DLB WARNING: (%d:%d) - Process %d trying to Retrieve cpus when have already %d\n", node, me, process, cpus[process]);
		}else{
			lended=0;
			i= (process+1)%procs;

			if(idleCpus>=(_default_nthreads)){
				lended=_default_nthreads;
				idleCpus-=lended;
			}else{
				lended=idleCpus;
				idleCpus=0;
			}

			while ((lended<(_default_nthreads)) && (i!=process)){
				if (cpus[i]>=2*(_default_nthreads)){
					lended=_default_nthreads;
					cpus[i]=cpus[i]-_default_nthreads;
					changed=i;
				}else if (cpus[i]>_default_nthreads){
					lended=cpus[i]-_default_nthreads;
					cpus[i]=_default_nthreads;
					changed=i;
				}
				i= (i+1)%procs;
			}
			cpus[process]=_default_nthreads;
		}
	}
	return changed;
}
/******* Slave Thread Functions ********/
void* slaveThread_Lend_simple(void* arg){
	int cpus;

#ifdef debugConfig
	fprintf(stderr,"DLB DEBUG: (%d:%d) - Creating Slave thread\n", node, me);
#endif	
	StartSlaveComm();
	threadsUsed=_default_nthreads;

	while(!finished){
		GetFromMaster((char*)&cpus,sizeof(int));
		threads2use=cpus;
	}
        return NULL;
}

void Lend_simple_updateresources(){
	if (threadsUsed!=threads2use){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", node, me, threads2use);
#endif
		update_threads(threads2use);
		threadsUsed=threads2use;
	}
}

