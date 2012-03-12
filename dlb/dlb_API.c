#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <pthread.h>

#include <dlb_API.h>

#include <LB_arch/MyTime.h>
#include <LB_policies/Lend.h>
#include <LB_policies/Lend_light.h>
#include <LB_policies/LeWI_active.h>
#include <LB_policies/Lend_simple.h>
#include <LB_policies/Weight.h>
#include <LB_policies/JustProf.h>
#include <LB_policies/DWB_Eco.h>
#include <LB_policies/Lewi_map.h>

#include <LB_arch/arch.h>
#include <LB_numThreads/numThreads.h>
#include <LB_MPI/tracing.h>

int iterNum;
struct timespec initAppl;
struct timespec initComp;
struct timespec initMPI;

struct timespec iterCpuTime;
struct timespec iterMPITime;

struct timespec CpuTime;
struct timespec MPITime;

double iterCpuTime_avg=0, iterMPITime_avg=0 ;
char prof;
int ready=0;

BalancePolicy lb_funcs;

int nodeId, meId, procsNode;

int use_dpd;

void Init(int me, int num_procs, int node){
	//Read Environment vars
	char* policy;
	char* profile;
	prof=0;

	use_dpd=0;

	nodeId=node;
	meId=me;
	procsNode=num_procs;

	iterNum=0;

	if ((policy=getenv("LB_POLICY"))==NULL){
		fprintf(stderr,"DLB PANIC: LB_POLICY must be defined\n");
		exit(1);
	}

	if ((profile=getenv("LB_PROFILE"))!=NULL){
		if (strcasecmp(profile, "YES")==0){
			prof=1;
		}
	}

//	update_threads(CPUS_NODE/num_procs);

	if (strcasecmp(policy, "LEND_simple")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Lend_simple cpus\n", node, me);
#endif	
		lb_funcs.init = &Lend_simple_Init;
		lb_funcs.finish = &Lend_simple_Finish;
		lb_funcs.initIteration = &Lend_simple_InitIteration;
		lb_funcs.finishIteration = &Lend_simple_FinishIteration;
		lb_funcs.intoCommunication = &Lend_simple_IntoCommunication;
		lb_funcs.outOfCommunication = &Lend_simple_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Lend_simple_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Lend_simple_OutOfBlockingCall;
		lb_funcs.updateresources = &Lend_simple_updateresources;

	}else if (strcasecmp(policy, "LeWI_t")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Lend cpus\n", node, me);
#endif	
		lb_funcs.init = &Lend_Init;
		lb_funcs.finish = &Lend_Finish;
		lb_funcs.initIteration = &Lend_InitIteration;
		lb_funcs.finishIteration = &Lend_FinishIteration;
		lb_funcs.intoCommunication = &Lend_IntoCommunication;
		lb_funcs.outOfCommunication = &Lend_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Lend_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Lend_OutOfBlockingCall;
		lb_funcs.updateresources = &Lend_updateresources;

	}else if (strcasecmp(policy, "LeWI_A")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI_Active \n", node, me);
#endif	
		lb_funcs.init = &LeWI_A_Init;
		lb_funcs.finish = &LeWI_A_Finish;
		lb_funcs.initIteration = &LeWI_A_InitIteration;
		lb_funcs.finishIteration = &LeWI_A_FinishIteration;
		lb_funcs.intoCommunication = &LeWI_A_IntoCommunication;
		lb_funcs.outOfCommunication = &LeWI_A_OutOfCommunication;
		lb_funcs.intoBlockingCall = &LeWI_A_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &LeWI_A_OutOfBlockingCall;
		lb_funcs.updateresources = &LeWI_A_updateresources;

	}else if (strcasecmp(policy, "LeWI")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Lend cpus light version\n", node, me);
#endif	
		
		lb_funcs.init = &Lend_light_Init;
		lb_funcs.finish = &Lend_light_Finish;
		lb_funcs.initIteration = &Lend_light_InitIteration;
		lb_funcs.finishIteration = &Lend_light_FinishIteration;
		lb_funcs.intoCommunication = &Lend_light_IntoCommunication;
		lb_funcs.outOfCommunication = &Lend_light_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Lend_light_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Lend_light_OutOfBlockingCall;
		lb_funcs.updateresources = &Lend_light_updateresources;

	}else if (strcasecmp(policy, "Map")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI with Map of cpus version\n", node, me);
#endif	
		
		lb_funcs.init = &Map_Init;
		lb_funcs.finish = &Map_Finish;
		lb_funcs.initIteration = &Map_InitIteration;
		lb_funcs.finishIteration = &Map_FinishIteration;
		lb_funcs.intoCommunication = &Map_IntoCommunication;
		lb_funcs.outOfCommunication = &Map_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Map_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Map_OutOfBlockingCall;
		lb_funcs.updateresources = &Map_updateresources;

	}else if (strcasecmp(policy, "WEIGHT")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Weight balancing\n", node, me);
#endif	
		use_dpd=1;

		lb_funcs.init = &Weight_Init;
		lb_funcs.finish = &Weight_Finish;
		lb_funcs.initIteration = &Weight_InitIteration;
		lb_funcs.finishIteration = &Weight_FinishIteration;
		lb_funcs.intoCommunication = &Weight_IntoCommunication;
		lb_funcs.outOfCommunication = &Weight_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Weight_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Weight_OutOfBlockingCall;
		lb_funcs.updateresources = &Weight_updateresources;

	}else if (strcasecmp(policy, "DWB_Eco")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: DWB_Eco balancing\n", node, me);
#endif	
		use_dpd=1;

		lb_funcs.init = &DWB_Eco_Init;
		lb_funcs.finish = &DWB_Eco_Finish;
		lb_funcs.initIteration = &DWB_Eco_InitIteration;
		lb_funcs.finishIteration = &DWB_Eco_FinishIteration;
		lb_funcs.intoCommunication = &DWB_Eco_IntoCommunication;
		lb_funcs.outOfCommunication = &DWB_Eco_OutOfCommunication;
		lb_funcs.intoBlockingCall = &DWB_Eco_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &DWB_Eco_OutOfBlockingCall;
		lb_funcs.updateresources = &DWB_Eco_updateresources;

	}else if (strcasecmp(policy, "NO")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - No Load balancing\n", node, me);
#endif	
		lb_funcs.init = &dummyInit;
		lb_funcs.finish = &dummyFunc;
		lb_funcs.initIteration = &dummyFunc;
		lb_funcs.finishIteration = &dummyFinishIter;
		lb_funcs.intoCommunication = &dummyFunc;
		lb_funcs.outOfCommunication = &dummyFunc;
		lb_funcs.intoBlockingCall = &dummyIntoBlockingCall;
		lb_funcs.outOfBlockingCall = &dummyFunc;
		lb_funcs.updateresources = &dummyFunc;
	}else{
		fprintf(stderr,"DLB PANIC: Unknown policy: %s\n", policy);
		exit(1);
	}

#ifdef debugBasicInfo
	if (me==0 && node==0){
		fprintf(stdout, "DLB: Balancing policy: %s balancing\n", policy);
		fprintf(stdout, "DLB: MPI processes per node: %d \n", num_procs);
		fprintf(stdout, "DLB: Each MPI process starts with %d threads\n", CPUS_NODE/num_procs);
		
		if (prof){
			fprintf(stdout, "DLB: Profiling of application active\n");
		}
	}
#endif

	if (prof){
		clock_gettime(CLOCK_REALTIME, &initAppl);
		reset(&iterCpuTime);
		reset(&iterMPITime);
		reset(&CpuTime);
		reset(&MPITime);
		clock_gettime(CLOCK_REALTIME, &initComp);
	}

	lb_funcs.init(me, num_procs, node);
	ready=1;

}


void Finish(void){
	lb_funcs.finish();
	if (prof){
		struct timespec aux, aux2;

		clock_gettime(CLOCK_REALTIME, &aux);
		diff_time(initComp, aux, &aux2);
		add_time(CpuTime, aux2, &CpuTime);

		add_time(CpuTime, iterCpuTime, &CpuTime);
		add_time(MPITime, iterMPITime, &MPITime);

		diff_time(initAppl, aux, &aux);
		
		if (meId==0 && nodeId==0){
			fprintf(stdout, "DLB: Application time: %.4f\n", to_secs(aux));
			fprintf(stdout, "DLB: Iterations detected: %d\n", iterNum);
		}
		fprintf(stdout, "DLB: (%d:%d) - CPU time: %.4f\n", nodeId, meId, to_secs(CpuTime));
		fprintf(stdout, "DLB: (%d:%d) - MPI time: %.4f\n", nodeId, meId, to_secs(MPITime));
	}
}

void InitIteration(void){
	add_time(CpuTime, iterCpuTime, &CpuTime);
	add_time(MPITime, iterMPITime, &MPITime);

	iterCpuTime_avg=(to_secs(iterCpuTime)+iterCpuTime_avg)/2;
	iterMPITime_avg=(to_secs(iterMPITime)+iterMPITime_avg)/2;

	reset(&iterCpuTime);
	reset(&iterMPITime);

	lb_funcs.initIteration();
	iterNum++;
}

void FinishIteration(){
	double cpuSecs;
	double MPISecs;

	cpuSecs=to_secs(iterCpuTime);
	MPISecs=to_secs(iterMPITime);

	lb_funcs.finishIteration(cpuSecs, MPISecs);
}

void IntoCommunication(void){
	struct timespec aux;
	clock_gettime(CLOCK_REALTIME, &initMPI);
	diff_time(initComp, initMPI, &aux);
	add_time(iterCpuTime, aux, &iterCpuTime);

	lb_funcs.intoCommunication();
}

void OutOfCommunication(void){
	struct timespec aux;
	clock_gettime(CLOCK_REALTIME, &initComp);
	diff_time(initMPI, initComp, &aux);
	add_time(iterMPITime, aux, &iterMPITime);

	lb_funcs.outOfCommunication();
}

void IntoBlockingCall(void){
	double cpuSecs;
	double MPISecs;
	cpuSecs=iterCpuTime_avg;
	MPISecs=iterMPITime_avg;

	lb_funcs.intoBlockingCall(cpuSecs, MPISecs);
}

void OutOfBlockingCall(void){
	lb_funcs.outOfBlockingCall();
}

void updateresources(){
	if(ready){
		add_event(RUNTIME_EVENT, 4);
		lb_funcs.updateresources();
		add_event(RUNTIME_EVENT, 0);
	}
}

void updateresources_(){
	if(ready){
		add_event(RUNTIME_EVENT, 4);
		lb_funcs.updateresources();
		add_event(RUNTIME_EVENT, 0);
	}
}

void UpdateResources(){
	if(ready){
		add_event(RUNTIME_EVENT, 4);
		lb_funcs.updateresources();
		add_event(RUNTIME_EVENT, 0);
	}
}
void UpdateResources_Map(int max_cpus){
	if(ready){
		add_event(RUNTIME_EVENT, 4);
		Map_updateresources(max_cpus);
		add_event(RUNTIME_EVENT, 0);
	}
}


int tracing_ready(){
	return ready;
}
void dummyFunc(){}
void dummyInit(int me, int num_procs, int node){}
void dummyFinishIter(double a, double b){}
void dummyIntoBlockingCall(double a, double b){}
