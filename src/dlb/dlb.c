/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

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
#include <LB_policies/PERaL.h>

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

int omp_get_max_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_num_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_max_threads(void) __attribute__( ( weak ) );
const char* nanos_get_pm(void) __attribute__( ( weak ) );


char prof;
int ready=0;

BalancePolicy lb_funcs;

int use_dpd;

static void dummyFunc(){}

void Init(){
	//Read Environment vars
	char* policy;

	char* thread_distrib;
	prof=0;

	use_dpd=0;
	//iterNum=0;

	if ((policy=getenv("LB_POLICY"))==NULL){
		fprintf(stderr,"DLB PANIC: LB_POLICY must be defined\n");
		exit(1);
	}

        parse_env_bool( "LB_JUST_BARRIER", &_just_barrier );

        parse_env_bool( "LB_AGGRESSIVE_INIT", &_aggressive_init );

        parse_env_bool( "LB_PRIORIZE_LOCALITY", &_priorize_locality );

        parse_env_blocking_mode( "LB_LEND_MODE", &_blocking_mode );

	if (strcasecmp(policy, "LeWI")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI\n", _node_id, _process_id);
#endif	
		
		lb_funcs.init = &Lend_light_Init;
		lb_funcs.finish = &Lend_light_Finish;
		lb_funcs.intoCommunication = &Lend_light_IntoCommunication;
		lb_funcs.outOfCommunication = &Lend_light_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Lend_light_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Lend_light_OutOfBlockingCall;
		lb_funcs.updateresources = &Lend_light_updateresources;
		lb_funcs.returnclaimed = &dummyFunc;

	}else if (strcasecmp(policy, "Map")==0){
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI with Map of cpus version\n", _node_id, _process_id);
#endif	
		
		lb_funcs.init = &Map_Init;
		lb_funcs.finish = &Map_Finish;
		lb_funcs.intoCommunication = &Map_IntoCommunication;
		lb_funcs.outOfCommunication = &Map_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Map_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Map_OutOfBlockingCall;
		lb_funcs.updateresources = &Map_updateresources;
		lb_funcs.returnclaimed = &dummyFunc;

	}else if (strcasecmp(policy, "WEIGHT")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Weight balancing\n", _node_id, _process_id);
#endif	
		use_dpd=1;

		lb_funcs.init = &Weight_Init;
		lb_funcs.finish = &Weight_Finish;
		lb_funcs.intoCommunication = &Weight_IntoCommunication;
		lb_funcs.outOfCommunication = &Weight_OutOfCommunication;
		lb_funcs.intoBlockingCall = &Weight_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &Weight_OutOfBlockingCall;
		lb_funcs.updateresources = &Weight_updateresources;
		lb_funcs.returnclaimed = &dummyFunc;

	}else if (strcasecmp(policy, "LeWI_mask")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI mask\n", _node_id, _process_id);
#endif
		lb_funcs.init = &lewi_mask_Init;
		lb_funcs.finish = &lewi_mask_Finish;
		lb_funcs.intoCommunication = &lewi_mask_IntoCommunication;
		lb_funcs.outOfCommunication = &lewi_mask_OutOfCommunication;
		lb_funcs.intoBlockingCall = &lewi_mask_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &lewi_mask_OutOfBlockingCall;
		lb_funcs.updateresources = &lewi_mask_UpdateResources;
		lb_funcs.returnclaimed = &lewi_mask_ReturnClaimedCpus;

	}else if (strcasecmp(policy, "RaL")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - Balancing policy: RaL: Redistribute and Lend \n", _node_id, _process_id);
#endif
		use_dpd=1;

		if (_mpis_per_node>1){
			lb_funcs.init = &PERaL_Init;
			lb_funcs.finish = &PERaL_Finish;
			lb_funcs.intoCommunication = &PERaL_IntoCommunication;
			lb_funcs.outOfCommunication = &PERaL_OutOfCommunication;
			lb_funcs.intoBlockingCall = &PERaL_IntoBlockingCall;
			lb_funcs.outOfBlockingCall = &PERaL_OutOfBlockingCall;
			lb_funcs.updateresources = &PERaL_UpdateResources;
			lb_funcs.returnclaimed = &PERaL_ReturnClaimedCpus;
		}else{
			lb_funcs.init = &RaL_Init;
			lb_funcs.finish = &RaL_Finish;
			lb_funcs.intoCommunication = &RaL_IntoCommunication;
			lb_funcs.outOfCommunication = &RaL_OutOfCommunication;
			lb_funcs.intoBlockingCall = &RaL_IntoBlockingCall;
			lb_funcs.outOfBlockingCall = &RaL_OutOfBlockingCall;
			lb_funcs.updateresources = &RaL_UpdateResources;
			lb_funcs.returnclaimed = &RaL_ReturnClaimedCpus;
		}
	}else if (strcasecmp(policy, "NO")==0){
#ifdef debugConfig
		fprintf(stderr, "DLB: (%d:%d) - No Load balancing\n", _node_id,  _process_id);
#endif	
		use_dpd=1;

		lb_funcs.init = &JustProf_Init;
		lb_funcs.finish = &JustProf_Finish;
		lb_funcs.intoCommunication = &JustProf_IntoCommunication;
		lb_funcs.outOfCommunication = &JustProf_OutOfCommunication;
		lb_funcs.intoBlockingCall = &JustProf_IntoBlockingCall;
		lb_funcs.outOfBlockingCall = &JustProf_OutOfBlockingCall;
		lb_funcs.updateresources = &JustProf_UpdateResources;
		lb_funcs.returnclaimed = &dummyFunc;
	}else{
		fprintf(stderr,"DLB PANIC: Unknown policy: %s\n", policy);
		exit(1);
	}

	debug_basic_info0 ( "DLB: Balancing policy: %s balancing\n", policy);
	debug_basic_info0 ( "DLB: MPI processes per node: %d \n", _node_id);

	if ((thread_distrib=getenv("LB_THREAD_DISTRIBUTION"))==NULL){
		if ( nanos_get_pm ) {
			const char *pm = nanos_get_pm();
			if ( strcmp( pm, "OpenMP" ) == 0 ) {
				_default_nthreads = nanos_omp_get_max_threads();
			}else if ( strcmp( pm, "OmpSs" ) == 0 ) {
				_default_nthreads = nanos_omp_get_num_threads();
			}else{
				fatal( "Unknown Programming Model\n" );
			}
		}else{
			_default_nthreads = omp_get_max_threads();
		}

		debug_basic_info0 ( "DLB: Each MPI process starts with %d threads\n", _default_nthreads);

	//Initial thread distribution specified
	}else{

		char* token = strtok(thread_distrib, "-");
		int i=0;
		while(i<_process_id && (token = strtok(NULL, "-"))){
			i++;
		}
			
		if (i!=_process_id){
			warning ("Error parsing the LB_THREAD_DISTRIBUTION (%s), using default\n");
				if ( nanos_get_pm ) {
        	                const char *pm = nanos_get_pm();
                	        if ( strcmp( pm, "OpenMP" ) == 0 ) {
                        	        _default_nthreads = nanos_omp_get_max_threads();
	                        }else if ( strcmp( pm, "OmpSs" ) == 0 ) {
        	                        _default_nthreads = nanos_omp_get_num_threads();
                	        }else{
                        	        fatal( "Unknown Programming Model\n" );
	                        }
        	        }else{
                	        _default_nthreads = omp_get_max_threads();
                	}

                	debug_basic_info0 ( "DLB: Each MPI process starts with %d threads\n", _default_nthreads);

		}else{
			_default_nthreads=atoi(token);
			debug_basic_info ( "DLB: I start with %d threads\n", _default_nthreads);
		}
	}




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

	lb_funcs.init();
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
		
		if (meId==0 && _node_idId==0){
			fprintf(stdout, "DLB: Application time: %.4f\n", to_secs(aux));
			fprintf(stdout, "DLB: Iterations detected: %d\n", iterNum);
		}
		fprintf(stdout, "DLB: (%d:%d) - CPU time: %.4f\n", _node_idId, meId, to_secs(CpuTime));
		fprintf(stdout, "DLB: (%d:%d) - MPI time: %.4f\n", _node_idId, meId, to_secs(MPITime));
	}*/
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

void IntoBlockingCall(int is_iter, int is_single){
/*	double cpuSecs;
	double MPISecs;
	cpuSecs=iterCpuTime_avg;
	MPISecs=iterMPITime_avg;*/
	if (is_single)
		lb_funcs.intoBlockingCall(is_iter, ONE_CPU);
	else
		lb_funcs.intoBlockingCall(is_iter, _blocking_mode );
}

void OutOfBlockingCall(int is_iter){
	lb_funcs.outOfBlockingCall(is_iter);
}

void updateresources( int max_resources ){
	if(ready){
		add_event(RUNTIME_EVENT, EVENT_UPDATE);
		lb_funcs.updateresources( max_resources );
		add_event(RUNTIME_EVENT, 0);
	}
}

void returnclaimed( void ){
	if(ready){
		add_event(RUNTIME_EVENT, EVENT_RETURN);
		lb_funcs.returnclaimed();
		add_event(RUNTIME_EVENT, 0);
	}
}

int tracing_ready(){
	return ready;
}
