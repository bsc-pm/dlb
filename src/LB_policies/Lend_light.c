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

#include <Lend_light.h>
#include <LB_numThreads/numThreads.h>
#include <LB_comm/comm_lend_light.h>
#include "support/globals.h"
#include "support/mask_utils.h"
#include "support/utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/resource.h>

int greedy;
int default_cpus;
int myCPUS;

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(){
#ifdef debugConfig
	fprintf(stderr, "DLB DEBUG: (%d:%d) - Lend_light Init\n", _node_id, _process_id);
#endif
	char* policy_greedy;
	_process_id = _process_id;
	_node_id = _node_id;
	default_cpus = _default_nthreads;
	
	setThreads_Lend_light(default_cpus);

#ifdef debugBasicInfo
	    if (_process_id==0 && _node_id==0){
	      fprintf(stdout, "DLB: Default cpus per process: %d\n", default_cpus);
	    }
#endif

	char* bind;
	if ((bind=getenv("LB_BIND"))!=NULL){
	    	if(strcasecmp(bind, "YES")==0){
	      		fprintf(stdout, "DLB: Binding of threads to cpus enabled\n");
			if (css_set_num_threads){ //If we are running with openMP we must bind the slave threads
				bind_master(_process_id, _node_id);
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
	    if (_process_id==0 && _node_id==0){
	      fprintf(stdout, "DLB: Policy mode GREEDY\n");
	    }
#endif
	    }
	}

//Setting blocking mode

	//Initialize shared _process_idmory
	ConfigShMem(_mpis_per_node, _process_id, _node_id, default_cpus, greedy);

      if ( _aggressive_init ) {
         setThreads_Lend_light( mu_get_system_size() );
         setThreads_Lend_light( _default_nthreads );
      }
}

void Lend_light_Finish(void){
	if (_process_id==0) finalize_comm();
}

void Lend_light_IntoCommunication(void){}

void Lend_light_OutOfCommunication(void){}

void Lend_light_IntoBlockingCall(int is_iter, int blocking_mode){

	if ( blocking_mode == ONE_CPU ) {
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", _node_id, _process_id, myCPUS-1);
#endif
		releaseCpus(myCPUS-1);
		setThreads_Lend_light(1);
	}else{
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - LENDING %d cpus\n", _node_id, _process_id, myCPUS);
#endif
		releaseCpus(myCPUS);
		setThreads_Lend_light(0);
	}
}

void Lend_light_OutOfBlockingCall(int is_iter){

	int cpus=acquireCpus(myCPUS);
	setThreads_Lend_light(cpus);
#ifdef debugLend
	fprintf(stderr, "DLB DEBUG: (%d:%d) - ACQUIRING %d cpus\n", _node_id, _process_id, cpus);
#endif

}

/******* Auxiliar Functions Lend_light Balancing Policy ********/

void Lend_light_updateresources(){
	int cpus = checkIdleCpus(myCPUS);
	if (myCPUS!=cpus){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", _node_id, _process_id, cpus);
#endif
		 setThreads_Lend_light(cpus);
	}
}

void setThreads_Lend_light(int numThreads){

	if (myCPUS!=numThreads){
#ifdef debugDistribution
		fprintf(stderr,"DLB DEBUG: (%d:%d) - Using %d cpus\n", _node_id, _process_id, numThreads);
#endif
		update_threads(numThreads);
		myCPUS=numThreads;
	}
}
