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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MPI

#include <stdio.h>
#include "LB_core/DLB_kernel.h"
#include "support/tracing.h"
#include <MPI_calls_coded.h>
#include <DPD/DPD.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bits/local_lim.h>
#include "support/globals.h"
#include "support/debug.h"

int omp_get_max_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_num_threads(void) __attribute__( ( weak ) );
int nanos_omp_get_max_threads(void) __attribute__( ( weak ) );
const char* nanos_get_pm(void) __attribute__( ( weak ) );

int periodo; 
static int mpi_ready = 0;
static int is_iter;

void before_init(void){
	DPDWindowSize(300);
}

void after_init(void){
	add_event(RUNTIME_EVENT, EVENT_INIT);
	is_iter=0;

        MPI_Comm_rank( MPI_COMM_WORLD, &_mpi_rank );
        MPI_Comm_size( MPI_COMM_WORLD, &_mpi_size );

	char hostname[HOST_NAME_MAX];
	char recvData[_mpi_size][HOST_NAME_MAX];

	if (gethostname(hostname, HOST_NAME_MAX)<0){
		perror("gethostname");
	}

	MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
	int error_code = PMPI_Allgather (hostname, HOST_NAME_MAX, MPI_CHAR, recvData, HOST_NAME_MAX, MPI_CHAR, MPI_COMM_WORLD);

	if (error_code != MPI_SUCCESS) {
		char error_string[BUFSIZ];
   		int length_of_error_string;

   		MPI_Error_string(error_code, error_string, &length_of_error_string);
                fatal( "%3d: %s\n", _mpi_rank, error_string );
	}

        int i;
        _mpis_per_node = 0;
        for ( i=0; i<_mpi_size; i++ ) {
           if ( strcmp ( recvData[i], hostname ) == 0 )
              _mpis_per_node++;
        }

	int procsIds[_mpi_size][2];
	if (_mpi_rank==0){
		int j, maxSetNode;
		int nodes=_mpi_size/_mpis_per_node;
		int procsPerNode[nodes];
		char nodesIds[nodes][HOST_NAME_MAX];

		maxSetNode=0;
		for (i=0; i<nodes; i++){
			memset(nodesIds[i], 0, HOST_NAME_MAX);
			procsPerNode[i]=0;
		}

		strcpy(nodesIds[0],recvData[0]);
		procsPerNode[0]=1;
		procsIds[0][0]=0;
		procsIds[0][1]=0;
		maxSetNode++;

		for(i=1; i<_mpi_size; i++){
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
	}

        int data[2];
        PMPI_Scatter(procsIds, 2, MPI_INT, data, 2, MPI_INT, 0, MPI_COMM_WORLD);
        _process_id = data[0];
        _node_id    = data[1];

        /********************************************
         * _node_id    = _mpi_rank / _mpis_per_node;
         * _process_id = _mpi_rank % _mpis_per_node;
         ********************************************/

        // Color = node, key is 0 because we don't mind the internal rank
        MPI_Comm_split( MPI_COMM_WORLD, _node_id, 0, &_mpi_comm_node );

	Init();
	mpi_ready=1;	
	add_event(RUNTIME_EVENT, 0);
}

void before_mpi(mpi_call call_type, intptr_t buf, intptr_t dest){
	int valor_dpd;	
	if(mpi_ready){
		add_event(RUNTIME_EVENT, EVENT_INTO_MPI);
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
		add_event(RUNTIME_EVENT, EVENT_OUT_MPI);

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

void node_barrier(void) { if (mpi_ready) MPI_Barrier( _mpi_comm_node ); }

#endif /* HAVE_MPI */
