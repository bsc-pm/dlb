#include <stdio.h>
#include <process_MPI.h>
#include <mpi.h>
#include <MPI_calls_coded.h>

void DLB_mpi_init_enter (int *argc, char ***argv){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_init...............\n");
	#endif
	before_init();
}

void DLB_mpi_init_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_init...............\n");
	#endif
	after_init();
}

void DLB_mpi_finalize_enter (){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_finalize...............\n");
	#endif
	before_finalize();
}

void DLB_mpi_finalize_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_finalize...............\n");
	#endif
	after_finalize();
}

void DLB_mpi_sendrecv_enter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_sendrecv...............\n");
	#endif
	
	before_mpi(SendRecv, (int)sendbuf, dest);
}

void DLB_mpi_sendrecv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_sendRecv...............\n");
	#endif
	
	after_mpi(SendRecv);
}

void DLB_mpi_sendrecv_replace_enter ( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_sendrecv_replace...............\n");
	#endif
	
	before_mpi(SendRecv, (int)buf, dest);
}

void DLB_mpi_sendrecv_replace_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_sendRecv_replace...............\n");
	#endif
	
	after_mpi(SendRecv);
}

void DLB_mpi_bsend_enter (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_bsend...............\n");
	#endif
	
	before_mpi(Bsend, (int)buf, dest);
}

void DLB_mpi_bsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_bsend...............\n");
	#endif
	
	after_mpi(Bsend);
}

void DLB_mpi_ssend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_ssend...............\n");
	#endif
	
	before_mpi(Ssend, (int)buf, dest);
}

void DLB_mpi_ssend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_ssend...............\n");
	#endif
	
	after_mpi(Ssend);
}

void DLB_mpi_rsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_rsend...............\n");
	#endif
	
	before_mpi(Rsend, (int)buf, dest);
}

void DLB_mpi_rsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_rsend...............\n");
	#endif
	
	after_mpi(Rsend);
}

void DLB_mpi_send_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_send...............\n");
	#endif
	
	before_mpi(Send, (int)buf, dest);
}

void DLB_mpi_send_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_send...............\n");
	#endif
	
	after_mpi(Send);
}

void DLB_mpi_ibsend_enter (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_ibsend...............\n");
	#endif

	before_mpi(Ibsend, (int)buf, dest);	
}

void DLB_mpi_ibsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_ibsend...............\n");
	#endif
	
	after_mpi(Ibsend);
}

void DLB_mpi_isend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_isend...............\n");
	#endif
	
	before_mpi(Isend, (int)buf, dest);

}

void DLB_mpi_isend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_isend...............\n");
	#endif
	
	after_mpi(Isend);
}

void DLB_mpi_issend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_issend...............\n");
	#endif

	before_mpi(Issend, (int)buf, dest);
}

void DLB_mpi_issend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_issend...............\n");
	#endif

	after_mpi(Issend);
}

void DLB_mpi_irsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_irsend...............\n");
	#endif

	before_mpi(Irsend, (int)buf, dest);
}

void DLB_mpi_irsend_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_irsend...............\n");
	#endif

	after_mpi(Irsend);
}

void DLB_mpi_recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_recv...............\n");
	#endif
	
	before_mpi(Recv, (int)buf, source);
}

void DLB_mpi_recv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_recv...............\n");
	#endif
	
	after_mpi(Recv);
}

void DLB_mpi_irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_irecv...............\n");
	#endif

	before_mpi(Irecv, (int)buf, source);
}

void DLB_mpi_irecv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_irecv...............\n");
	#endif

	after_mpi(Irecv);
}

void DLB_mpi_reduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_reduce...............\n");
	#endif
	
	before_mpi(Reduce, (int)sendbuf, root);
}

void DLB_mpi_reduce_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_reduce...............\n");
	#endif
	
	after_mpi(Reduce);
}
								
void DLB_mpi_reduce_scatter_enter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_reduce_scatter...............\n");
	#endif
	
	before_mpi(Reduce_scatter, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_reduce_scatter_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_reduce_scatter...............\n");
	#endif
	
	after_mpi(Reduce_scatter);
}

void DLB_mpi_allreduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_allreduce...............\n");
	#endif
	
	before_mpi(Allreduce, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_allreduce_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_allreduce...............\n");
	#endif
	
	after_mpi(Allreduce);
}

void DLB_mpi_barrier_enter (MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_barrier...............\n");
	#endif
	
	before_mpi(Barrier, 0, 0);
}

void DLB_mpi_barrier_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_barrier...............\n");
	#endif
	
	after_mpi(Barrier);
}

void DLB_mpi_wait_enter (MPI_Request  *request, MPI_Status   *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_wait...............\n");
	#endif
	
	before_mpi(Wait, 0, 0);
}

void DLB_mpi_wait_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_wait...............\n");
	#endif
	
	after_mpi(Wait);
}

void DLB_mpi_waitall_enter (int count, MPI_Request *array_of_Requests, MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_waitall...............\n");
	#endif
	
	before_mpi(Waitall, 0, 0);
}

void DLB_mpi_waitall_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_waitall...............\n");
	#endif
	
	after_mpi(Waitall);
}

void DLB_mpi_waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_waitany...............\n");
	#endif
	
	before_mpi(Waitany, 0, 0);
}

void DLB_mpi_waitany_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_waitany...............\n");
	#endif
	
	after_mpi(Waitany);
}

void DLB_mpi_waitsome_enter (int incount,MPI_Request *array_of_Requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_waitsome...............\n");
	#endif
	
	before_mpi(Waitsome, 0, 0);
}

void DLB_mpi_waitsome_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_waitsome...............\n");
	#endif
	
	after_mpi(Waitsome);
}

void DLB_mpi_bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_bcast...............\n");
	#endif
	
	before_mpi(Bcast, (int)buffer, root);
}


void DLB_mpi_bcast_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_bcast...............\n");
	#endif
	
	after_mpi(Bcast);
}

void DLB_mpi_alltoall_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_alltoall...............\n");
	#endif
	
	before_mpi(Alltoall, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_alltoall_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_alltoall...............\n");
	#endif
	
	after_mpi(Alltoall);
}

void DLB_mpi_alltoallv_enter (void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_alltoallv...............\n");
	#endif
	
	before_mpi(Alltoallv, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_alltoallv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_alltoallv...............\n");
	#endif
	
	after_mpi(Alltoallv);
}

void DLB_mpi_allgather_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_allgather...............\n");
	#endif
	
	before_mpi(Allgather, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_allgather_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_allgather...............\n");
	#endif
	
	after_mpi(Allgather);
}

void DLB_mpi_allgatherv_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_allgatherv...............\n");
	#endif
	
	before_mpi(Allgatherv, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_allgatherv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_allgatherv...............\n");
	#endif
	
	after_mpi(Allgatherv);
}

void DLB_mpi_gather_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_gather...............\n");
	#endif
	
	before_mpi(Gather, (int)sendbuf, root);
}

void DLB_mpi_gather_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_gather...............\n");
	#endif
	
	after_mpi(Gather);
}

void DLB_mpi_gatherv_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_gatherv...............\n");
	#endif
	
	before_mpi(Gatherv, (int)sendbuf, root);
}

void DLB_mpi_gatherv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_gatherv...............\n");
	#endif
	
	after_mpi(Gatherv);
}

void DLB_mpi_scatter_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_scatter...............\n");
	#endif
	
	before_mpi(Scatter, (int)sendbuf, root);
}

void DLB_mpi_scatter_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_scatter...............\n");
	#endif
	
	after_mpi(Scatter);
}

void DLB_mpi_scatterv_enter (void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_scatterv...............\n");
	#endif
	
	before_mpi(Scatterv, (int)sendbuf, root);
}

void DLB_mpi_scatterv_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_scatterv...............\n");
	#endif
	
	after_mpi(Scatterv);
}

void DLB_mpi_scan_enter (void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_scan...............\n");
	#endif
	
	before_mpi(Scan, (int)sendbuf, (int)recvbuf);
}

void DLB_mpi_scan_leave (){
	#ifdef debugInOut
	fprintf(stderr," << MPI_scan...............\n");
	#endif
	
	after_mpi(Scan);
}
