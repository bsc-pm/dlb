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

#include <stdio.h>
#include "dlb/dlb.h"
#include "support/tracing.h"
#include <MPI_calls_coded.h>
#include <DPD/DPD.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "support/globals.h"
#include "support/debug.h"

int omp_get_max_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_num_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_max_threads(void) __attribute__( ( weak ) );
const char* nanos_get_pm(void) __attribute__( ( weak ) );

int periodo; 
int me;
int mpi_ready=0;
static MPI_Comm mpi_comm_node;
static int is_iter;

void before_init(void){
	DPDWindowSize(300);
}

void after_init(void){
	add_event(RUNTIME_EVENT, 1);
	is_iter=0;
	int num_mpis=0, node;

	MPI_Comm_rank(MPI_COMM_WORLD,&me);
//fprintf(stderr, "%d: I am %d\n", getpid(), me);

	//Setting me and nodeId for MPIs
	char nodeId[50];
	int procs;
	MPI_Comm_size (MPI_COMM_WORLD, &procs); 
	char recvData[procs][50];

	if (gethostname(nodeId, 50)<0){
		perror("gethostname");
	}

	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
	int error_code = PMPI_Allgather (nodeId, 50, MPI_CHAR, recvData, 50, MPI_CHAR, MPI_COMM_WORLD);

	if (error_code != MPI_SUCCESS) {
		char error_string[BUFSIZ];
   		int length_of_error_string;

   		MPI_Error_string(error_code, error_string, &length_of_error_string);
   		fprintf(stderr, "%3d: %s\n", me, error_string);
	}

        int i;
        for ( i=0; i<procs; i++ ) {
           if ( strcmp ( recvData[i], nodeId ) == 0 )
              num_mpis++;
        }

	if (me==0){
		int j, maxSetNode;
		int nodes=procs/num_mpis;
		int procsPerNode[nodes];
		char nodesIds[nodes][50];
		int procsIds[procs][2];

		maxSetNode=0;
		for (i=0; i<nodes; i++){
			memset(nodesIds[i], 0, 50);
			procsPerNode[i]=0;
		}

		strcpy(nodesIds[0],recvData[0]);
		procsPerNode[0]=1;
		procsIds[0][0]=0;
		procsIds[0][1]=0;
		maxSetNode++;

		for(i=1; i<procs; i++){
			j=0;
			while((strcmp(recvData[i],nodesIds[j]))&&(j<nodes)){
				j++;
			}
			
			if(j>=nodes){
				strcpy(nodesIds[maxSetNode],recvData[i]);
				procsIds[i][0]=procsPerNode[maxSetNode];
				procsIds[i][1]=maxSetNode;
				procsPerNode[maxSetNode]++;
				maxSetNode++;
			}else{
				strcpy(nodesIds[j],recvData[i]);
				procsIds[i][0]=procsPerNode[j];
				procsIds[i][1]=j;
				procsPerNode[j]++;
			}
		}
		for(i=1; i<procs; i++){
			PMPI_Send((void*)procsIds[i], 2, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		me=procsIds[0][0];
		node=procsIds[0][1];
	}else{
		int data[2];
		PMPI_Recv(&data, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, 0);
		me=data[0];
		node=data[1];
	}

	/////////////////////////////////////
	//node = me/num_mpis;
	//me = me % num_mpis;
	/////////////////////////////////////
#ifdef debugConfig
	fprintf(stderr, "DLB: (%d:%d) - MPIs per node: %d\n", node, me, num_mpis);
#endif	

        // Color = node, key is 0 because we don't mind the internal rank
        MPI_Comm_split( MPI_COMM_WORLD, node, 0, &mpi_comm_node );

        // Globals
        MPI_Comm_rank( MPI_COMM_WORLD, &_mpi_rank );
        _process_id = me;
        _node_id = node;
        _mpis_per_node = num_mpis;

/*        if ( nanos_get_pm ) {
           const char *pm = nanos_get_pm();
           if ( strcmp( pm, "OpenMP" ) == 0 ) {
              _default_nthreads = nanos_omp_get_max_threads();
           }
           else if ( strcmp( pm, "OmpSs" ) == 0 ) {
              _default_nthreads = nanos_omp_get_num_threads();
           }
           else
              fatal( "Unknown Programming Model\n" );
        }
        else {
           _default_nthreads = omp_get_max_threads();
        }*/

	Init();
	mpi_ready=1;	
	add_event(RUNTIME_EVENT, 0);
}

void before_mpi(mpi_call call_type, intptr_t buf, intptr_t dest){
	int valor_dpd;	
	if(mpi_ready){
		add_event(RUNTIME_EVENT, 2);
		IntoCommunication();

		if(use_dpd){
			long value = (long)((((buf>>5)^dest)<<5)|call_type);
		
			valor_dpd=DPD(value,&periodo);
			//Only update if already treated previous iteration
			if(is_iter==0)	is_iter=valor_dpd;

		}

		if(_just_barrier){
			if (call_type==Barrier){
				debug_blocking_MPI( " >> MPI_Barrier...............\n" );
				IntoBlockingCall(is_iter, 0);
			}
		}else if (is_blocking(call_type)){
			debug_blocking_MPI( " >> %s...............\n", mpi_call_names[call_type] );
			IntoBlockingCall(is_iter, 0);
		}
	
		add_event(RUNTIME_EVENT, 0);
	}
}

void after_mpi(mpi_call call_type){
	if (mpi_ready){
		add_event(RUNTIME_EVENT, 3);

		if(_just_barrier){
			if (call_type==Barrier){
				debug_blocking_MPI( " << MPI_Barrier...............\n" );
				OutOfBlockingCall(is_iter);
				is_iter=0;
			}
		}else if (is_blocking(call_type)){
			debug_blocking_MPI( " << %s...............\n", mpi_call_names[call_type] );
			OutOfBlockingCall(is_iter);
			is_iter=0;
		}
	
		OutOfCommunication();
		add_event(RUNTIME_EVENT, 0);
	}
}

void before_finalize(void){
	Finish();
	mpi_ready=0;
}

void after_finalize(void){}

void enable_mpi(void) { mpi_ready = 1; }

void disable_mpi(void) { mpi_ready = 0; }

void node_barrier(void) { if (mpi_ready) MPI_Barrier( mpi_comm_node ); }
