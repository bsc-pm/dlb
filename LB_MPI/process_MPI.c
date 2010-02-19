#include <stdio.h>
#include <dlb/dlb_API.h>
#include <LB_MPI/tracing.h>
#include <MPI_calls_coded.h>
#include <DPD/DPD.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int periodo; 
int me;

void before_init(){
	DPDWindowSize(300);
}

void after_init(){
	add_event(RUNTIME, 1);
	char* mpi_per_node;
	int num_mpis, node;

	if ((mpi_per_node=getenv("LB_MPIxNODE"))==NULL){
			fprintf(stdout,"PANIC: LB_MPIxNODE must be defined\n");
			exit(1);
	}
	num_mpis= atoi(mpi_per_node);	
	MPI_Comm_rank(MPI_COMM_WORLD,&me);

	//Setting me and nodeId for MPIs
	char nodeId[50];
	int procs;
	MPI_Comm_size (MPI_COMM_WORLD, &procs); 
	char recvData[procs][50];

	/*if (me==0){
		recvData=malloc(sizeof(char)*50*procs);
	}*/

	if (gethostname(nodeId, 50)<0){
		perror("gethostname");
	}

	MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
	int error_code = PMPI_Gather (nodeId, 50, MPI_CHAR, recvData, 50, MPI_CHAR, 0, MPI_COMM_WORLD);

	if (error_code != MPI_SUCCESS) {
		char error_string[BUFSIZ];
   		int length_of_error_string;

   		MPI_Error_string(error_code, error_string, &length_of_error_string);
   		fprintf(stderr, "%3d: %s\n", me, error_string);
	}

	if (me==0){
		int i, j, maxSetNode;
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
	fprintf(stderr, "%d:%d - mpi_per_node: %d\n", node, me, num_mpis);
#endif	

	Init(me, num_mpis, node);
		
	add_event(RUNTIME, 0);
}

void before_mpi(mpi_call call_type, intptr_t buf, intptr_t dest){
	add_event(RUNTIME, 2);
	int valor_dpd;
	IntoCommunication();

	if (is_blocking(call_type)){
		IntoBlockingCall();
	}

	long value = (long)((((buf>>5)^dest)<<5)|call_type);

	valor_dpd=DPD(value,&periodo);
	//fprintf(stderr, "%d - Muestra %d Llamada %0x Valor DPD %d\n", me, value, call_type, valor_dpd);

	if (valor_dpd!=0){
//		fprintf(stderr, "%d - Detecto iteracion\n", me);
		FinishIteration();
		InitIteration();
	}
	add_event(RUNTIME, 0);
}

void after_mpi(mpi_call call_type){
	add_event(RUNTIME, 3);

	if (is_blocking(call_type)){
		OutOfBlockingCall();
	}

	OutOfCommunication();
	add_event(RUNTIME, 0);
}

void before_finalize(){
	Finish();
}

void after_finalize(){}


