#ifndef MPI_INTERFACE_H
#define MPI_INTERFACE_H

void DLB_MPI_Init_enter (int *argc, char ***argv);

void DLB_MPI_Init_leave ();

void DLB_MPI_Finalize_enter ();

void DLB_MPI_Finalize_leave ();

void DLB_MPI_Bsend_enter (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Bsend_leave ();

void DLB_MPI_Ssend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Ssend_leave ();

void DLB_MPI_Rsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Rsend_leave ();

void DLB_MPI_Send_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Send_leave ();

void DLB_MPI_Ibsend_enter (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request );

void DLB_MPI_Ibsend_leave ();

void DLB_MPI_Isend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Isend_leave ();

void DLB_MPI_Issend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Issend_leave ();

void DLB_MPI_Irsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Irsend_leave ();

void DLB_MPI_Recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);

void DLB_MPI_Recv_leave ();

void DLB_MPI_Irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Irecv_leave ();

void DLB_MPI_Reduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm);

void DLB_MPI_Reduce_leave ();
								
void DLB_MPI_Reduce_scatter_enter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Reduce_scatter_leave ();

void DLB_MPI_Allreduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Allreduce_leave ();

void DLB_MPI_Barrier_enter (MPI_Comm comm);

void DLB_MPI_Barrier_leave ();

void DLB_MPI_Wait_enter (MPI_Request  *request, MPI_Status   *status);

void DLB_MPI_Wait_leave ();

void DLB_MPI_Waitall_enter (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses);

void DLB_MPI_Waitall_leave ();

void DLB_MPI_Waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status);

void DLB_MPI_Waitany_leave ();

void DLB_MPI_Waitsome_enter (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses);

void DLB_MPI_Waitsome_leave ();

void DLB_MPI_Bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);

void DLB_MPI_Bcast_leave ();

void DLB_MPI_Alltoall_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Alltoall_leave ();

void DLB_MPI_Alltoallv_enter (void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Alltoallv_leave ();


void DLB_MPI_Allgather_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Allgather_leave ();

void DLB_MPI_Allgatherv_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Allgatherv_leave ();

void DLB_MPI_Gather_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Gather_leave ();
// 
void DLB_MPI_Gatherv_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm );

void DLB_MPI_Gatherv_leave ();

void DLB_MPI_Scatter_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Scatter_leave ();

void DLB_MPI_Scatterv_enter (void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Scatterv_leave ();

void DLB_MPI_Scan_enter (void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Scan_leave ();

#endif //MPI_INTERFACE_H


