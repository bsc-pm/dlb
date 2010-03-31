#include <Lend.h>
#include <LB_arch/arch.h>
#include <LB_numThreads/numThreads.h>
#include <LB_MPI/tracing.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

int me, node, procs;
int finished;
int threadsUsed;
int idleCpus;
int greedy;
int default_cpus;


/******* Main Functions Lend Balancing Policy ********/

void Lend_Init(int meId, int num_procs, int nodeId){
#ifdef debugConfig
	fprintf(stderr, "%d:%d - Lend Init\n", nodeId, meId);
#endif
	char* policy_greedy;

	me = meId;
	node = nodeId;
	procs = num_procs;
	default_cpus=CPUS_NODE/procs;
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

	//Create auxiliar threads
	createThreads_Lend();	
}

void Lend_Finish(void){
	finished=1;
	comm_close();
}

void Lend_InitIteration(){}

void Lend_FinishIteration(double cpuSecs, double MPISecs){}

void Lend_IntoCommunication(void){
}

void Lend_OutOfCommunication(void){
//	Lend_updateresources();
}

void Lend_IntoBlockingCall(double cpuSecs, double MPISecs){
	ProcMetrics info;
#ifdef debugLend
	fprintf(stderr, "%d:%d - LENDING %d cpus\n", node, me, threadsUsed);
#endif
	info.action = LEND_CPUS;
	info.cpus=threadsUsed;
	info.secsComp=cpuSecs;
	info.secsMPI=MPISecs;

	setThreads_Lend(0);
	SendToMaster((char *)&info,sizeof(info));
}

void Lend_OutOfBlockingCall(void){
	ProcMetrics info;

#ifdef debugLend
	fprintf(stderr, "%d:%d - ACQUIRING %d cpus\n", node, me, threadsUsed);
#endif

	info.action = ACQUIRE_CPUS;
	info.cpus=threadsUsed;

	setThreads_Lend(default_cpus);
	SendToMaster((char *)&info,sizeof(info));
}

/******* Auxiliar Functions Lend Balancing Policy ********/

/* Creates auxiliar threads */
void createThreads_Lend(){
	pthread_t t;
	finished=0;

#ifdef debugConfig
	fprintf(stderr, "%d:%d - Creating Threads\n", node, me);
#endif
	LoadCommConfig(procs, me, node);

	if (me==0){ 
		if (pthread_create(&t,NULL,masterThread_Lend,NULL)>0){
			perror("createThreads:Error in pthread_create master\n");
			exit(1);
		}
	}

	StartSlaveComm();
	setThreads_Lend(default_cpus);
	threadsUsed=default_cpus;

	/*if (pthread_create(&t,NULL,slaveThread_Lend,NULL)>0){
		perror("createThreads:Error in pthread_create slave\n");
		exit(1);
	}*/
}

/******* Master Thread Functions ********/
void* masterThread_Lend(void* arg){
	int cpus[procs], numThreads;
	int i;
	//int info;
	ProcMetrics info;
	ProcMetrics old_info[procs];

#ifdef debugConfig
	fprintf(stderr,"%d:%d - Creating Master thread\n", node, me);
#endif	

	StartMasterComm();
	idleCpus=0;
	

	//We start with equidistribution
	for (i=0; i<procs; i++) cpus[i]=default_cpus;

	applyNewDistribution(cpus, procs+1);

	while(!finished){
		i = GetFromAnySlave((char*)&info, sizeof(info));
		if (i>=0){
			if (info.action ==LEND_CPUS){
				numThreads=0;
				WriteToSlave(i,(char*)&numThreads,sizeof(int));
				old_info[i]=info;
				assign_lended_cpus(cpus, i, old_info);
			}else if(info.action ==ACQUIRE_CPUS){
				numThreads=default_cpus;
				WriteToSlave(i,(char*)&numThreads,sizeof(int));
				retrieve_cpus(cpus, i, old_info);
			}/*else{
				fprintf(stderr,"%d:%d - WHAT?? %d\n", node, me, info.action);
			}*/
	
	#ifdef debugDistribution
				int aux=0;
				fprintf(stderr,"%d:%d - New Distribution: ", node, me);
				for (i=0; i<procs; i++){ fprintf(stderr,"[%d]", cpus[i]);aux+=cpus[i];}
				fprintf(stderr,"Total=%d\n", aux);
	#endif
		}
	}
}

void applyNewDistribution(int * cpus, int except){
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

void assign_lended_cpus(int* cpus, int process, ProcMetrics info[]){
	int i;
	int lended;
	double load;
	double max;
	int loadedProc;

	if(cpus[process]==0){
		fprintf(stderr,"%d:%d - WARNING proces %d trying to Lend cpus when does not have any\n", node, me, process);
	}else{
		lended=cpus[process];

		

		while (lended>0){
			max=-1;
			loadedProc=-1;

			i=(process+1)%procs;
			while(i!=process){
				if(cpus[i]!=0){
					load=(double)info[i].secsComp/(double)cpus[i];
				}else{
					load=-1;
				}
//fprintf(stderr,"%d:%d - Process %d has load %.4f (%.4f/%d)\n", node, me, i, load, info[i].secsComp, cpus[i]);
				if (load>max){
					max=load;
					loadedProc=i;
				}
				i=(i+1)%procs;
			}
			
			if (loadedProc!=-1){
				cpus[loadedProc]++;
				lended--;
				WriteToSlave(loadedProc,(char*) &cpus[loadedProc] , sizeof(int));
#ifdef debugLend
	fprintf(stderr,"%d:%d - Process %d LENDED %d cpus to process %d\n", node, me, process, 1, loadedProc);
#endif
			}else{
				idleCpus+=lended;
				lended=0;
			}
			
		}
		cpus[process]=0;
	}
}

void retrieve_cpus(int* cpus, int process, ProcMetrics info[]){
	if(cpus[process]!=0){
			fprintf(stderr,"%d:%d - WARNING proces %d trying to Retrieve cpus when have already %d\n", node, me, process, cpus[process]);
	}else{
		int assigned=0;
		int missing=default_cpus;
		int i;
		if(idleCpus>=(default_cpus)){
			if (greedy){
			    assigned=idleCpus;
			    idleCpus=0;
			}else{
			    assigned=missing;
			    idleCpus-=missing;
			}
			missing=0;
#ifdef debugLend
		fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, default_cpus);
#endif
		}else{

#ifdef debugLend
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, idleCpus);
#endif
			missing=missing-idleCpus;
			idleCpus=0;

			i= (process+1)%procs;
			while ((missing!=0) && (i!=process)){
					
				if (cpus[i]>default_cpus){
					if (missing>(cpus[i]-default_cpus)){
#ifdef debugLend
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, cpus[i]-default_cpus, i);
#endif
						missing=missing-(cpus[i]-default_cpus);
						cpus[i]=default_cpus;
					}else{
#ifdef debugLend
	fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, missing, i);
#endif
						cpus[i]=cpus[i]-missing;
						missing=0;
					}
					WriteToSlave(i,(char*) &cpus[i] , sizeof(int));
				}
				i= (i+1)%procs;
			}
			assigned=default_cpus;
		}
		if(missing!=0){
			fprintf(stderr,"%d:%d - WARNING proces %d tried to retrieve %d cpus and there are not enough available\n", node, me, process, default_cpus);
		}
		cpus[process]=assigned;
		WriteToSlave(process,(char*) &cpus[process] , sizeof(int));
	}
}

void Lend_updateresources(){
	int cpus;
	GetFromMasterNonBlocking((char*)&cpus,sizeof(int));

	if (threadsUsed!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, cpus);
#endif
		update_threads(cpus);
		threadsUsed=cpus;
	}
}

void setThreads_Lend(int numThreads){

	if (threadsUsed!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, numThreads);
#endif
		update_threads(numThreads);
		threadsUsed=numThreads;
	}
}
