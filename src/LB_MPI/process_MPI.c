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

int omp_get_max_threads() __attribute__( ( weak ) );
int nanos_omp_get_num_threads() __attribute__( ( weak ) );

int periodo; 
int me;
int just_barrier;
int mpi_ready=0;

void before_init(void){
	DPDWindowSize(300);
}

void after_init(void){
	add_event(RUNTIME_EVENT, 1);
	int num_mpis=0, node;

	just_barrier=0;
	if ((getenv("LB_JUST_BARRIER"))==NULL){
		just_barrier=0;
	}else{
		just_barrier=1;
		fprintf(stdout, "DLB: Only lending resources when MPI_Barrier (Env. var. LB_JUST_BARRIER is set)\n");
	}

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

        // Globals
        MPI_Comm_rank( MPI_COMM_WORLD, &_mpi_rank );
        _process_id = me;
        _node_id = node;
        _mpis_per_node = num_mpis;
        if ( nanos_omp_get_num_threads ) _default_nthreads = nanos_omp_get_num_threads();
        else if ( omp_get_max_threads ) _default_nthreads = omp_get_max_threads();
        else _default_nthreads = 1;

	Init(me, num_mpis, node);
	mpi_ready=1;	
	add_event(RUNTIME_EVENT, 0);
}

void before_mpi(mpi_call call_type, intptr_t buf, intptr_t dest){
	if(mpi_ready){
		add_event(RUNTIME_EVENT, 2);
		int valor_dpd;
		IntoCommunication();

		if(just_barrier){
			if (call_type==Barrier){
				IntoBlockingCall();
			}
		}else if (is_blocking(call_type)){
			IntoBlockingCall();
		}
	
		if(use_dpd){
			long value = (long)((((buf>>5)^dest)<<5)|call_type);
		
			valor_dpd=DPD(value,&periodo);
		
			if (valor_dpd!=0){
				FinishIteration();
				InitIteration();
			}
		}
		add_event(RUNTIME_EVENT, 0);
	}
}

void after_mpi(mpi_call call_type){
	if (mpi_ready){
		add_event(RUNTIME_EVENT, 3);

		if(just_barrier){
			if (call_type==Barrier){
				OutOfBlockingCall();
			}
		}else if (is_blocking(call_type)){
			OutOfBlockingCall();
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
