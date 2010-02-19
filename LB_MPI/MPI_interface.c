#include <stdio.h>
#include <process_MPI.h>
#include <mpi.h>
#include <MPI_calls_coded.h>
#include <stdint.h>

void DLB_MPI_Init_enter (int *argc, char ***argv){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Init...............\n");
	#endif
	before_init();
}

void DLB_MPI_Init_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Init...............\n");
	#endif
	after_init();
}

void DLB_MPI_Finalize_enter (){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Finalize...............\n");
	#endif
	before_finalize();
}

void DLB_MPI_Finalize_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Finalize...............\n");
	#endif
	after_finalize();
}

void DLB_MPI_Sendrecv_enter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Sendrecv...............\n");
	#endif
	
	before_mpi(SendRecv, (intptr_t)sendbuf, dest);
}

void DLB_MPI_Sendrecv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_SendRecv...............\n");
	#endif
	
	after_mpi(SendRecv);
}

void DLB_MPI_Sendrecv_replace_enter ( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Sendrecv_replace...............\n");
	#endif
	
	before_mpi(SendRecv, (intptr_t)buf, dest);
}

void DLB_MPI_Sendrecv_replace_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_SendRecv_replace...............\n");
	#endif
	
	after_mpi(SendRecv);
}

void DLB_MPI_Bsend_enter (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Bsend...............\n");
	#endif
	
	before_mpi(Bsend, (intptr_t)buf, dest);
}

void DLB_MPI_Bsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Bsend...............\n");
	#endif
	
	after_mpi(Bsend);
}

void DLB_MPI_Ssend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Ssend...............\n");
	#endif
	
	before_mpi(Ssend, (intptr_t)buf, dest);
}

void DLB_MPI_Ssend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Ssend...............\n");
	#endif
	
	after_mpi(Ssend);
}

void DLB_MPI_Rsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Rsend...............\n");
	#endif
	
	before_mpi(Rsend, (intptr_t)buf, dest);
}

void DLB_MPI_Rsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Rsend...............\n");
	#endif
	
	after_mpi(Rsend);
}

void DLB_MPI_Send_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Send...............\n");
	#endif
	
	before_mpi(Send, (intptr_t)buf, dest);
}

void DLB_MPI_Send_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Send...............\n");
	#endif
	
	after_mpi(Send);
}

void DLB_MPI_Ibsend_enter (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Ibsend...............\n");
	#endif

	before_mpi(Ibsend, (intptr_t)buf, dest);	
}

void DLB_MPI_Ibsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Ibsend...............\n");
	#endif
	
	after_mpi(Ibsend);
}

void DLB_MPI_Isend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Isend...............\n");
	#endif
	
	before_mpi(Isend, (intptr_t)buf, dest);

}

void DLB_MPI_Isend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Isend...............\n");
	#endif
	
	after_mpi(Isend);
}

void DLB_MPI_Issend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Issend...............\n");
	#endif

	before_mpi(Issend, (intptr_t)buf, dest);
}

void DLB_MPI_Issend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Issend...............\n");
	#endif

	after_mpi(Issend);
}

void DLB_MPI_Irsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Irsend...............\n");
	#endif

	before_mpi(Irsend, (intptr_t)buf, dest);
}

void DLB_MPI_Irsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Irsend...............\n");
	#endif

	after_mpi(Irsend);
}

void DLB_MPI_Recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Recv...............\n");
	#endif
	
	before_mpi(Recv, (intptr_t)buf, source);
}

void DLB_MPI_Recv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Recv...............\n");
	#endif
	
	after_mpi(Recv);
}

void DLB_MPI_Irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Irecv...............\n");
	#endif

	before_mpi(Irecv, (intptr_t)buf, source);
}

void DLB_MPI_Irecv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Irecv...............\n");
	#endif

	after_mpi(Irecv);
}

void DLB_MPI_Reduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Reduce...............\n");
	#endif
	
	before_mpi(Reduce, (intptr_t)sendbuf, root);
}

void DLB_MPI_Reduce_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Reduce...............\n");
	#endif
	
	after_mpi(Reduce);
}
								
void DLB_MPI_Reduce_scatter_enter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Reduce_scatter...............\n");
	#endif
	
	before_mpi(Reduce_scatter, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Reduce_scatter_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Reduce_scatter...............\n");
	#endif
	
	after_mpi(Reduce_scatter);
}

void DLB_MPI_Allreduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allreduce...............\n");
	#endif
	
	before_mpi(Allreduce, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Allreduce_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allreduce...............\n");
	#endif
	
	after_mpi(Allreduce);
}

void DLB_MPI_Barrier_enter (MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Barrier...............\n");
	#endif
	
	before_mpi(Barrier, 0, 0);
}

void DLB_MPI_Barrier_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Barrier...............\n");
	#endif
	
	after_mpi(Barrier);
}

void DLB_MPI_Wait_enter (MPI_Request  *request, MPI_Status   *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Wait...............\n");
	#endif
	
	before_mpi(Wait, 0, 0);
}

void DLB_MPI_Wait_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Wait...............\n");
	#endif
	
	after_mpi(Wait);
}

void DLB_MPI_Waitall_enter (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitall...............\n");
	#endif
	
	before_mpi(Waitall, 0, 0);
}

void DLB_MPI_Waitall_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitall...............\n");
	#endif
	
	after_mpi(Waitall);
}

void DLB_MPI_Waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitany...............\n");
	#endif
	
	before_mpi(Waitany, 0, 0);
}

void DLB_MPI_Waitany_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitany...............\n");
	#endif
	
	after_mpi(Waitany);
}

void DLB_MPI_Waitsome_enter (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitsome...............\n");
	#endif
	
	before_mpi(Waitsome, 0, 0);
}

void DLB_MPI_Waitsome_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitsome...............\n");
	#endif
	
	after_mpi(Waitsome);
}

void DLB_MPI_Bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Bcast...............\n");
	#endif
	
	before_mpi(Bcast, (intptr_t)buffer, root);
}


void DLB_MPI_Bcast_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Bcast...............\n");
	#endif
	
	after_mpi(Bcast);
}

void DLB_MPI_Alltoall_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Alltoall...............\n");
	#endif
	
	before_mpi(Alltoall, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Alltoall_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Alltoall...............\n");
	#endif
	
	after_mpi(Alltoall);
}

void DLB_MPI_Alltoallv_enter (void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Alltoallv...............\n");
	#endif
	
	before_mpi(Alltoallv, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Alltoallv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Alltoallv...............\n");
	#endif
	
	after_mpi(Alltoallv);
}

void DLB_MPI_Allgather_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allgather...............\n");
	#endif
	
	before_mpi(Allgather, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Allgather_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allgather...............\n");
	#endif
	
	after_mpi(Allgather);
}

void DLB_MPI_Allgatherv_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allgatherv...............\n");
	#endif
	
	before_mpi(Allgatherv, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Allgatherv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allgatherv...............\n");
	#endif
	
	after_mpi(Allgatherv);
}

void DLB_MPI_Gather_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Gather...............\n");
	#endif
	
	before_mpi(Gather, (intptr_t)sendbuf, root);
}

void DLB_MPI_Gather_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Gather...............\n");
	#endif
	
	after_mpi(Gather);
}

void DLB_MPI_Gatherv_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Gatherv...............\n");
	#endif
	
	before_mpi(Gatherv, (intptr_t)sendbuf, root);
}

void DLB_MPI_Gatherv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Gatherv...............\n");
	#endif
	
	after_mpi(Gatherv);
}

void DLB_MPI_Scatter_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scatter...............\n");
	#endif
	
	before_mpi(Scatter, (intptr_t)sendbuf, root);
}

void DLB_MPI_Scatter_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scatter...............\n");
	#endif
	
	after_mpi(Scatter);
}

void DLB_MPI_Scatterv_enter (void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scatterv...............\n");
	#endif
	
	before_mpi(Scatterv, (intptr_t)sendbuf, root);
}

void DLB_MPI_Scatterv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scatterv...............\n");
	#endif
	
	after_mpi(Scatterv);
}

void DLB_MPI_Scan_enter (void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scan...............\n");
	#endif
	
	before_mpi(Scan, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

void DLB_MPI_Scan_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scan...............\n");
	#endif
	
	after_mpi(Scan);
}
