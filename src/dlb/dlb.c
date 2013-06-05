#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <pthread.h>

#include "dlb.h"

#include <LB_policies/Lend_light.h>
#include <LB_policies/Weight.h>
#include <LB_policies/JustProf.h>
#include <LB_policies/Lewi_map.h>
#include <LB_policies/lewi_mask.h>
#include <LB_policies/RaL.h>

#include <LB_numThreads/numThreads.h>
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/utils.h"
#include "support/mytime.h"
#include "support/mask_utils.h"

//int iterNum;
//struct timespec initAppl;
//struct timespec initComp;
//struct timespec initMPI;

//struct timespec iterCpuTime;
//struct timespec iterMPITime;

//struct timespec CpuTime;
//struct timespec MPITime;

//double iterCpuTime_avg=0, iterMPITime_avg=0 ;
char prof;
int ready=0;

BalancePolicy lb_funcs;

int nodeId, meId, procsNode;

int use_dpd;

//static void dummyFunc(){}

void Init(int me, int num_procs, int node){
	//Read Environment vars
	char* policy;
	prof=0;

	use_dpd=0;

	nodeId=node;
	meId=me;
	procsNode=num_procs;
        mpi_x_node=num_procs;

	//iterNum=0;

	if ((policy=getenv("LB_POLICY"))==NULL){
		fprintf(stderr,"DLB PANIC: LB_POLICY must be defined\n");
		exit(1);
	}

        parse_env_bool( "LB_JUST_BARRIER", &_just_barrier );

        parse_env_bool( "LB_AGGRESSIVE_INIT", &_aggressive_init );

        parse_env_blocking_mode( "LB_LEND_MODE", &_blocking_mode );

	if (strcasecmp(policy, "LeWI")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI\n", node, me);
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

	}else if (strcasecmp(policy, "LeWI_mask")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI mask\n", node, me);
#endif
		lb_funcs.init = &lewi_mask_init;
		lb_funcs.finish = &lewi_mask_finish;
		lb_funcs.initIteration = &lewi_mask_init_iteration;
		lb_funcs.finishIteration = &lewi_mask_finish_iteration;
		lb_funcs.intoCommunication = &lewi_mask_into_communication;
		lb_funcs.outOfCommunication = &lewi_mask_out_of_communication;
		lb_funcs.intoBlockingCall = &lewi_mask_into_blocking_call;
		lb_funcs.outOfBlockingCall = &lewi_mask_out_of_blocking_call;
		lb_funcs.updateresources = &lewi_mask_update_resources;

	}else if (strcasecmp(policy, "RaL")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: RaL: Redistribute and Lend \n", node, me);
#endif
		lb_funcs.init = &RaL_init;
		lb_funcs.finish = &RaL_finish;
		lb_funcs.initIteration = &RaL_init_iteration;
		lb_funcs.finishIteration = &RaL_finish_iteration;
		lb_funcs.intoCommunication = &RaL_into_communication;
		lb_funcs.outOfCommunication = &RaL_out_of_communication;
		lb_funcs.intoBlockingCall = &RaL_into_blocking_call;
		lb_funcs.outOfBlockingCall = &RaL_out_of_blocking_call;
		lb_funcs.updateresources = &RaL_update_resources;

	}else if (strcasecmp(policy, "NO")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - No Load balancing\n", node, me);
#endif	
		use_dpd=1;

		lb_funcs.init = &JustProf_Init;
		lb_funcs.finish = &JustProf_Finish;
		lb_funcs.initIteration = &JustProf_InitIteration;
		lb_funcs.finishIteration = &JustProf_FinishIteration;
		lb_funcs.intoCommunication = &JustProf_IntoCommunication;
		lb_funcs.outOfCommunication = &JustProf_OutOfCommunication;
		lb_funcs.intoBlockingCall = &JustProf_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &JustProf_OutOfBlockingCall;
		lb_funcs.updateresources = &JustProf_UpdateResources;
	}else{
		fprintf(stderr,"DLB PANIC: Unknown policy: %s\n", policy);
		exit(1);
	}

	debug_basic_info0 ( "DLB: Balancing policy: %s balancing\n", policy);
	debug_basic_info0 ( "DLB: MPI processes per node: %d \n", num_procs);
	debug_basic_info0 ( "DLB: Each MPI process starts with %d threads\n", _default_nthreads);

        if ( _just_barrier )
           debug_basic_info0 ( "Only lending resources when MPI_Barrier (Env. var. LB_JUST_BARRIER is set)\n" );

        if ( _blocking_mode == ONE_CPU )
           debug_basic_info0 ( "LEND mode set to 1CPU. I will leave a cpu per MPI process when in an MPI call\n" );
        else if ( _blocking_mode == BLOCK )
           debug_basic_info0 ( "LEND mode set to BLOCKING. I will lend all the resources when in an MPI call\n" );

        // FIXME: It could be printed from Nanos, mu_init is called twice now
        cpu_set_t default_mask;
        get_mask( &default_mask );
        mu_init();
        debug_basic_info ( "Default Mask: %s\n", mu_to_str(&default_mask) );
        //

/*	if (prof){
		clock_gettime(CLOCK_REALTIME, &initAppl);
		reset(&iterCpuTime);
		reset(&iterMPITime);
		reset(&CpuTime);
		reset(&MPITime);
		clock_gettime(CLOCK_REALTIME, &initComp);
	}*/

	lb_funcs.init(me, num_procs, node);
	ready=1;


}


void Finish(void){
	lb_funcs.finish();
/*	if (prof){
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
	}*/
}

void InitIteration(void){
/*	add_time(CpuTime, iterCpuTime, &CpuTime);
	add_time(MPITime, iterMPITime, &MPITime);

	iterCpuTime_avg=(to_secs(iterCpuTime)+iterCpuTime_avg)/2;
	iterMPITime_avg=(to_secs(iterMPITime)+iterMPITime_avg)/2;

	reset(&iterCpuTime);
	reset(&iterMPITime);*/

	lb_funcs.initIteration();
//	iterNum++;
}

void FinishIteration(){
/*	double cpuSecs;
	double MPISecs;

	cpuSecs=to_secs(iterCpuTime);
	MPISecs=to_secs(iterMPITime);*/

	lb_funcs.finishIteration();
}

void IntoCommunication(void){
/*	struct timespec aux;
	clock_gettime(CLOCK_REALTIME, &initMPI);
	diff_time(initComp, initMPI, &aux);
	add_time(iterCpuTime, aux, &iterCpuTime);*/

	lb_funcs.intoCommunication();
}

void OutOfCommunication(void){
/*	struct timespec aux;
	clock_gettime(CLOCK_REALTIME, &initComp);
	diff_time(initMPI, initComp, &aux);
	add_time(iterMPITime, aux, &iterMPITime);*/

	lb_funcs.outOfCommunication();
}

void IntoBlockingCall(void){
/*	double cpuSecs;
	double MPISecs;
	cpuSecs=iterCpuTime_avg;
	MPISecs=iterMPITime_avg;*/

	lb_funcs.intoBlockingCall();
}

void OutOfBlockingCall(void){
	lb_funcs.outOfBlockingCall();
}

void updateresources( int max_resources ){
	if(ready){
		add_event(RUNTIME_EVENT, 4);
		lb_funcs.updateresources( max_resources );
		add_event(RUNTIME_EVENT, 0);
	}
}

int tracing_ready(){
	return ready;
}
