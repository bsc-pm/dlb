#include <LeWI_active.h>
#include <LB_arch/arch.h>
#include <LB_numThreads/numThreads.h>

#include <pthread.h>
#include <stdio.h>

int me, node, procs;
int finished;
int threadsUsed;
int idleCpus;

/******* Main Functions LeWI_A Balancing Policy ********/

void LeWI_A_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "%d:%d - LeWI_A Init\n", nodeId, meId);
#endif
	me = meId;
	node = nodeId;
	procs = num_procs;

	//Create auxiliar threads
	createThreads_LeWI_A();	
}

void LeWI_A_Finish(void){
	finished=1;
	comm_close();
}

void LeWI_A_InitIteration(){}

void LeWI_A_FinishIteration(double cpuSecs, double MPISecs){}

void LeWI_A_IntoCommunication(void){
}

void LeWI_A_OutOfCommunication(void){
	//LeWI_A_updateresources();
}

void LeWI_A_IntoBlockingCall(double cpuSecs, double MPISecs){
	ProcMetrics info;
#ifdef debugLeWI_A
	fprintf(stderr, "%d:%d - LENDING %d cpus\n", node, me, threadsUsed);
#endif
	info.action = LEND_CPUS;
	info.cpus=threadsUsed;
	info.secsComp=cpuSecs;
	info.secsMPI=MPISecs;

#ifdef OMPI_events	
	OMPItrace_event (800001, 1);
#endif

	setThreads_LeWI_A(0);
	SendToMaster((char *)&info,sizeof(info));
#ifdef OMPI_events	
	OMPItrace_event (800001, 0);
#endif	
}

void LeWI_A_OutOfBlockingCall(void){
	ProcMetrics info;
	info.action = ACQUIRE_CPUS;
	info.cpus=threadsUsed;

#ifdef OMPI_events	
	OMPItrace_event (800001, 2);
#endif
	setThreads_LeWI_A(CPUS_NODE/procs);
	SendToMaster((char *)&info,sizeof(info));
#ifdef OMPI_events	
	OMPItrace_event (800001, 0);
#endif
#ifdef debugLeWI_A
	fprintf(stderr, "%d:%d - ACQUIRING %d cpus\n", node, me, threadsUsed);
#endif
}

/******* Auxiliar Functions LeWI_A Balancing Policy ********/

/* Creates auxiliar threads */
int createThreads_LeWI_A(){
	pthread_t t;
	finished=0;

#ifdef debugConfig
	fprintf(stderr, "%d:%d - Creating Threads\n", node, me);
#endif
	LoadCommConfig(procs, me, node);

	if (me==0){ 
		if (pthread_create(&t,NULL,masterThread_LeWI_A,NULL)>0){
			perror("createThreads:Error in pthread_create master\n");
			exit(1);
		}
	}

	StartSlaveComm();
	setThreads_LeWI_A(CPUS_NODE/procs);
	threadsUsed=CPUS_NODE/procs;

	/*if (pthread_create(&t,NULL,slaveThread_LeWI_A,NULL)>0){
		perror("createThreads:Error in pthread_create slave\n");
		exit(1);
	}*/
}

/******* Master Thread Functions ********/
void* masterThread_LeWI_A(void* arg){

	int cpus[procs], numThreads;
	int i, aux;
	//int info;
	ProcMetrics info;
	ProcMetrics old_info[procs];

#ifdef debugConfig
	fprintf(stderr,"%d:%d - Creating Master thread\n", node, me);
#endif	

	StartMasterComm();
	idleCpus=0;
	

	//We start with equidistribution
	for (i=0; i<procs; i++) cpus[i]=CPUS_NODE/procs;

	LeWI_A_applyNewDistribution(cpus, procs+1);

	while(!finished){
		i = GetFromAnySlave((char*)&info, sizeof(info));

		if (info.action ==LEND_CPUS){
			numThreads=0;
			WriteToSlave(i,(char*)&numThreads,sizeof(int));
			old_info[i]=info;
			idleCpus+=cpus[i];
			cpus[i]=0;
//			assign_lended_cpus(cpus, i, old_info);
		}else if(info.action ==ACQUIRE_CPUS){
			numThreads=CPUS_NODE/procs;
			WriteToSlave(i,(char*)&numThreads,sizeof(int));
			LeWI_A_retrieve_cpus(cpus, i, old_info);
		}else{
			cpus[i]+=idleCpus;
			idleCpus=0;
			SendToSlave(i,(char*) &cpus[i],sizeof(int));
		}

#ifdef debugDistribution
			aux=0;
			fprintf(stderr,"%d:%d - New Distribution: ", node, me);
			for (i=0; i<procs; i++){ fprintf(stderr,"[%d]", cpus[i]);aux+=cpus[i];}
			fprintf(stderr,"Total=%d\n", aux);
#endif
	}
}

void LeWI_A_applyNewDistribution(int * cpus, int except){
	int i;
#ifdef debugDistribution
	fprintf(stderr,"%d:%d - New Distribution: ", node, me);
	for (i=0; i<procs; i++) fprintf(stderr,"[%d]", cpus[i]);
	fprintf(stderr,"\n");
#endif
	for (i=0; i<procs; i++){
		if(i!=except){
			WriteToSlave(i,(char*) &cpus[i] , sizeof(int));
		}
	}
}

int LeWI_A_retrieve_cpus(int* cpus, int process, ProcMetrics info[]){
	if(cpus[process]!=0){
			fprintf(stderr,"%d:%d - WARNING proces %d trying to Retrieve cpus when have already %d\n", node, me, process, cpus[process]);
	}else{
		int missing=CPUS_NODE/procs;
		int i;
		if(idleCpus>=(CPUS_NODE/procs)){
			idleCpus-=missing;
			missing=0;
#ifdef debugLeWI_A
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, CPUS_NODE/procs);
#endif
		}else{

#ifdef debugLeWI_A
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, idleCpus);
#endif
			missing=missing-idleCpus;
			idleCpus=0;

			i= (process+1)%procs;
			while ((missing!=0) && (i!=process)){
					
				if (cpus[i]>CPUS_NODE/procs){
					if (missing>(cpus[i]-CPUS_NODE/procs)){
#ifdef debugLeWI_A
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, cpus[i]-CPUS_NODE/procs, i);
#endif
						missing=missing-(cpus[i]-CPUS_NODE/procs);
						cpus[i]=CPUS_NODE/procs;
					}else{
#ifdef debugLeWI_A
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, missing, i);
#endif
						cpus[i]=cpus[i]-missing;
						missing=0;
					}
					WriteToSlave(i,(char*) &cpus[i] , sizeof(int));
				}
				i= (i+1)%procs;
			}
		}
		cpus[process]=(CPUS_NODE/procs);
		//SendToSlave(process,(char*) &cpus[process] , sizeof(int));
		if(missing!=0){
			fprintf(stderr,"%d:%d - WARNING proces %d tried to retrieve %d cpus and there are not enough available\n", node, me, process, CPUS_NODE/procs);
		}
	}
}

void LeWI_A_updateresources(){
	int cpus;

	ProcMetrics info;
#ifdef debugLeWI_A
	fprintf(stderr, "%d:%d - Asking for cpus\n", node, me);
#endif
	info.action = ASK_4_CPUS;
	info.cpus=threadsUsed;

#ifdef OMPI_events	
	OMPItrace_event (800001, 3);
#endif

	SendToMaster((char *)&info,sizeof(info));
	GetFromMaster((char*)&cpus,sizeof(int));

#ifdef OMPI_events	
	OMPItrace_event (800001, 0);
#endif
	if (threadsUsed!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, cpus);
#endif
		update_threads(cpus);
		threadsUsed=cpus;
	}
}

// void LeWI_A_updateThreads(){
// 	int cpus;
// 
// 	GetFromMasterNonBlocking((char*)&cpus,sizeof(int));
// 
// 	if (threadsUsed!=cpus){
// #ifdef debugDistribution
// 		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, cpus);
// #endif
// 		update_threads(cpus);
// 		threadsUsed=cpus;
// 	}
// }

void setThreads_LeWI_A(int numThreads){

	if (threadsUsed!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, numThreads);
#endif
		update_threads(numThreads);
		threadsUsed=numThreads;
	}
}
