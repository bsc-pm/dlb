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
#include <process_MPI.h>
#include <mpi.h>
#include <MPI_calls_coded.h>
#include <stdint.h>

__attribute__((alias("DLB_MPI_Init_enter"))) void DLB_mpi_init_enter (int *argc, char ***argv);
void DLB_MPI_Init_enter (int *argc, char ***argv){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Init...............\n");
	#endif
	before_init();
}

__attribute__((alias("DLB_MPI_Init_leave"))) void DLB_mpi_init_leave (void);
void DLB_MPI_Init_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Init...............\n");
	#endif
	after_init();
}

__attribute__((alias("DLB_MPI_Finalize_enter"))) void DLB_mpi_finalize_enter (void);
void DLB_MPI_Finalize_enter (void){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Finalize...............\n");
	#endif
	before_finalize();
}

__attribute__((alias("DLB_MPI_Finalize_leave"))) void DLB_mpi_finalize_leave (void);
void DLB_MPI_Finalize_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Finalize...............\n");
	#endif
	after_finalize();
}

__attribute__((alias("DLB_MPI_Sendrecv_enter"))) void DLB_mpi_sendrecv_enter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status );
void DLB_MPI_Sendrecv_enter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Sendrecv (to: %d - from: %d)...............\n", dest, source);
	#endif
	
	before_mpi(SendRecv, (intptr_t)sendbuf, dest);
}

__attribute__((alias("DLB_MPI_Sendrecv_leave"))) void DLB_mpi_sendrecv_leave (void);
void DLB_MPI_Sendrecv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_SendRecv...............\n");
	#endif
	
	after_mpi(SendRecv);
}

__attribute__((alias("DLB_MPI_Sendrecv_replace_enter"))) void DLB_mpi_sendrecv_replace_enter ( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status );
void DLB_MPI_Sendrecv_replace_enter ( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Sendrecv_replace...............\n");
	#endif
	
	before_mpi(SendRecv, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Sendrecv_replace_leave"))) void DLB_mpi_sendrecv_replace_leave (void);
void DLB_MPI_Sendrecv_replace_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_SendRecv_replace...............\n");
	#endif
	
	after_mpi(SendRecv);
}

__attribute__((alias("DLB_MPI_Bsend_enter"))) void DLB_mpi_bsend_enter (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
void DLB_MPI_Bsend_enter (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Bsend...............\n");
	#endif
	
	before_mpi(Bsend, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Bsend_leave"))) void DLB_mpi_bsend_leave (void);
void DLB_MPI_Bsend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Bsend...............\n");
	#endif
	
	after_mpi(Bsend);
}

__attribute__((alias("DLB_MPI_Ssend_enter"))) void DLB_mpi_ssend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
void DLB_MPI_Ssend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Ssend...............\n");
	#endif
	
	before_mpi(Ssend, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Ssend_leave"))) void DLB_mpi_ssend_leave (void);
void DLB_MPI_Ssend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Ssend...............\n");
	#endif
	
	after_mpi(Ssend);
}

__attribute__((alias("DLB_MPI_Rsend_enter"))) void DLB_mpi_rsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
void DLB_MPI_Rsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Rsend...............\n");
	#endif
	
	before_mpi(Rsend, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Rsend_leave"))) void DLB_mpi_rsend_leave (void);
void DLB_MPI_Rsend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Rsend...............\n");
	#endif
	
	after_mpi(Rsend);
}

__attribute__((alias("DLB_MPI_Send_enter"))) void DLB_mpi_send_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
void DLB_MPI_Send_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Send...............\n");
	#endif
	
	before_mpi(Send, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Send_leave"))) void DLB_mpi_send_leave (void);
void DLB_MPI_Send_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Send...............\n");
	#endif
	
	after_mpi(Send);
}

__attribute__((alias("DLB_MPI_Ibsend_enter"))) void DLB_mpi_ibsend_enter (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request );
void DLB_MPI_Ibsend_enter (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Ibsend...............\n");
	#endif

	before_mpi(Ibsend, (intptr_t)buf, dest);	
}

__attribute__((alias("DLB_MPI_Ibsend_leave"))) void DLB_mpi_ibsend_leave (void);
void DLB_MPI_Ibsend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Ibsend...............\n");
	#endif
	
	after_mpi(Ibsend);
}

__attribute__((alias("DLB_MPI_Isend_enter"))) void DLB_mpi_isend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
void DLB_MPI_Isend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Isend...............\n");
	#endif
	
	before_mpi(Isend, (intptr_t)buf, dest);

}

__attribute__((alias("DLB_MPI_Isend_leave"))) void DLB_mpi_isend_leave (void);
void DLB_MPI_Isend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Isend...............\n");
	#endif
	
	after_mpi(Isend);
}

__attribute__((alias("DLB_MPI_Issend_enter"))) void DLB_mpi_issend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
void DLB_MPI_Issend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Issend...............\n");
	#endif

	before_mpi(Issend, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Issend_leave"))) void DLB_mpi_issend_leave (void);
void DLB_MPI_Issend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Issend...............\n");
	#endif

	after_mpi(Issend);
}

__attribute__((alias("DLB_MPI_Irsend_enter"))) void DLB_mpi_irsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
void DLB_MPI_Irsend_enter (void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Irsend...............\n");
	#endif

	before_mpi(Irsend, (intptr_t)buf, dest);
}

__attribute__((alias("DLB_MPI_Irsend_leave"))) void DLB_mpi_irsend_leave (void);
void DLB_MPI_Irsend_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Irsend...............\n");
	#endif

	after_mpi(Irsend);
}

__attribute__((alias("DLB_MPI_Recv_enter"))) void DLB_mpi_recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);
void DLB_MPI_Recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Recv...............\n");
	#endif
	
	before_mpi(Recv, (intptr_t)buf, source);
}

__attribute__((alias("DLB_MPI_Recv_leave"))) void DLB_mpi_recv_leave (void);
void DLB_MPI_Recv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Recv...............\n");
	#endif
	
	after_mpi(Recv);
}

__attribute__((alias("DLB_MPI_Irecv_enter"))) void DLB_mpi_irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request);
void DLB_MPI_Irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Irecv...............\n");
	#endif

	before_mpi(Irecv, (intptr_t)buf, source);
}

__attribute__((alias("DLB_MPI_Irecv_leave"))) void DLB_mpi_irecv_leave (void);
void DLB_MPI_Irecv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Irecv...............\n");
	#endif

	after_mpi(Irecv);
}

__attribute__((alias("DLB_MPI_Reduce_enter"))) void DLB_mpi_reduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm);
void DLB_MPI_Reduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Reduce...............\n");
	#endif
	
	before_mpi(Reduce, (intptr_t)sendbuf, root);
}

__attribute__((alias("DLB_MPI_Reduce_leave"))) void DLB_mpi_reduce_leave (void);
void DLB_MPI_Reduce_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Reduce...............\n");
	#endif
	
	after_mpi(Reduce);
}
								
__attribute__((alias("DLB_MPI_Reduce_scatter_enter"))) void DLB_mpi_reduce_scatter_enter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
void DLB_MPI_Reduce_scatter_enter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Reduce_scatter...............\n");
	#endif
	
	before_mpi(Reduce_scatter, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Reduce_scatter_leave"))) void DLB_mpi_reduce_scatter_leave (void);
void DLB_MPI_Reduce_scatter_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Reduce_scatter...............\n");
	#endif
	
	after_mpi(Reduce_scatter);
}

__attribute__((alias("DLB_MPI_Allreduce_enter"))) void DLB_mpi_allreduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
void DLB_MPI_Allreduce_enter (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allreduce...............\n");
	#endif
	
	before_mpi(Allreduce, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Allreduce_leave"))) void DLB_mpi_allreduce_leave (void);
void DLB_MPI_Allreduce_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allreduce...............\n");
	#endif
	
	after_mpi(Allreduce);
}

__attribute__((alias("DLB_MPI_Barrier_enter"))) void DLB_mpi_barrier_enter (MPI_Comm comm);
void DLB_MPI_Barrier_enter (MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Barrier...............\n");
	#endif
	
	before_mpi(Barrier, 0, 0);
}

__attribute__((alias("DLB_MPI_Barrier_leave"))) void DLB_mpi_barrier_leave (void);
void DLB_MPI_Barrier_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Barrier...............\n");
	#endif
	
	after_mpi(Barrier);
}

__attribute__((alias("DLB_MPI_Wait_enter"))) void DLB_mpi_wait_enter (MPI_Request  *request, MPI_Status   *status);
void DLB_MPI_Wait_enter (MPI_Request  *request, MPI_Status   *status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Wait...............\n");
	#endif
	
	before_mpi(Wait, 0, 0);
}

__attribute__((alias("DLB_MPI_Wait_leave"))) void DLB_mpi_wait_leave (void);
void DLB_MPI_Wait_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Wait...............\n");
	#endif
	
	after_mpi(Wait);
}

__attribute__((alias("DLB_MPI_Waitall_enter"))) void DLB_mpi_waitall_enter (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses);
void DLB_MPI_Waitall_enter (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitall...............\n");
	#endif
	
	before_mpi(Waitall, 0, 0);
}

__attribute__((alias("DLB_MPI_Waitall_leave"))) void DLB_mpi_waitall_leave (void);
void DLB_MPI_Waitall_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitall...............\n");
	#endif
	
	after_mpi(Waitall);
}

__attribute__((alias("DLB_MPI_Waitany_enter"))) void DLB_mpi_waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status);
void DLB_MPI_Waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitany...............\n");
	#endif
	
	before_mpi(Waitany, 0, 0);
}

__attribute__((alias("DLB_MPI_Waitany_leave"))) void DLB_mpi_waitany_leave (void);
void DLB_MPI_Waitany_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitany...............\n");
	#endif
	
	after_mpi(Waitany);
}

__attribute__((alias("DLB_MPI_Waitsome_enter"))) void DLB_mpi_waitsome_enter (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses);
void DLB_MPI_Waitsome_enter (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Waitsome...............\n");
	#endif
	
	before_mpi(Waitsome, 0, 0);
}

__attribute__((alias("DLB_MPI_Waitsome_leave"))) void DLB_mpi_waitsome_leave (void);
void DLB_MPI_Waitsome_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Waitsome...............\n");
	#endif
	
	after_mpi(Waitsome);
}

__attribute__((alias("DLB_MPI_Bcast_enter"))) void DLB_mpi_bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
void DLB_MPI_Bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Bcast...............\n");
	#endif
	
	before_mpi(Bcast, (intptr_t)buffer, root);
}


__attribute__((alias("DLB_MPI_Bcast_leave"))) void DLB_mpi_bcast_leave (void);
void DLB_MPI_Bcast_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Bcast...............\n");
	#endif
	
	after_mpi(Bcast);
}

__attribute__((alias("DLB_MPI_Alltoall_enter"))) void DLB_mpi_alltoall_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm);
void DLB_MPI_Alltoall_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Alltoall...............\n");
	#endif
	
	before_mpi(Alltoall, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Alltoall_leave"))) void DLB_mpi_alltoall_leave (void);
void DLB_MPI_Alltoall_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Alltoall...............\n");
	#endif
	
	after_mpi(Alltoall);
}

__attribute__((alias("DLB_MPI_Alltoallv_enter"))) void DLB_mpi_alltoallv_enter (void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm);
void DLB_MPI_Alltoallv_enter (void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Alltoallv...............\n");
	#endif
	
	before_mpi(Alltoallv, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Alltoallv_leave"))) void DLB_mpi_alltoallv_leave (void);
void DLB_MPI_Alltoallv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Alltoallv...............\n");
	#endif
	
	after_mpi(Alltoallv);
}

__attribute__((alias("DLB_MPI_Allgather_enter"))) void DLB_mpi_allgather_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm);
void DLB_MPI_Allgather_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allgather...............\n");
	#endif
	
	before_mpi(Allgather, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Allgather_leave"))) void DLB_mpi_allgather_leave (void);
void DLB_MPI_Allgather_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allgather...............\n");
	#endif
	
	after_mpi(Allgather);
}

__attribute__((alias("DLB_MPI_Allgatherv_enter"))) void DLB_mpi_allgatherv_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm);
void DLB_MPI_Allgatherv_enter (void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Allgatherv...............\n");
	#endif
	
	before_mpi(Allgatherv, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Allgatherv_leave"))) void DLB_mpi_allgatherv_leave (void);
void DLB_MPI_Allgatherv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Allgatherv...............\n");
	#endif
	
	after_mpi(Allgatherv);
}

__attribute__((alias("DLB_MPI_Gather_enter"))) void DLB_mpi_gather_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);
void DLB_MPI_Gather_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Gather...............\n");
	#endif
	
	before_mpi(Gather, (intptr_t)sendbuf, root);
}

__attribute__((alias("DLB_MPI_Gather_leave"))) void DLB_mpi_gather_leave (void);
void DLB_MPI_Gather_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Gather...............\n");
	#endif
	
	after_mpi(Gather);
}

__attribute__((alias("DLB_MPI_Gatherv_enter"))) void DLB_mpi_gatherv_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm );
void DLB_MPI_Gatherv_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm ){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Gatherv...............\n");
	#endif
	
	before_mpi(Gatherv, (intptr_t)sendbuf, root);
}

__attribute__((alias("DLB_MPI_Gatherv_leave"))) void DLB_mpi_gatherv_leave (void);
void DLB_MPI_Gatherv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Gatherv...............\n");
	#endif
	
	after_mpi(Gatherv);
}

__attribute__((alias("DLB_MPI_Scatter_enter"))) void DLB_mpi_scatter_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);
void DLB_MPI_Scatter_enter (void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scatter...............\n");
	#endif
	
	before_mpi(Scatter, (intptr_t)sendbuf, root);
}

__attribute__((alias("DLB_MPI_Scatter_leave"))) void DLB_mpi_scatter_leave (void);
void DLB_MPI_Scatter_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scatter...............\n");
	#endif
	
	after_mpi(Scatter);
}

__attribute__((alias("DLB_MPI_Scatterv_enter"))) void DLB_mpi_scatterv_enter (void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);
void DLB_MPI_Scatterv_enter (void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scatterv...............\n");
	#endif
	
	before_mpi(Scatterv, (intptr_t)sendbuf, root);
}

__attribute__((alias("DLB_MPI_Scatterv_leave"))) void DLB_mpi_scatterv_leave (void);
void DLB_MPI_Scatterv_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scatterv...............\n");
	#endif
	
	after_mpi(Scatterv);
}

__attribute__((alias("DLB_MPI_Scan_enter"))) void DLB_mpi_scan_enter (void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
void DLB_MPI_Scan_enter (void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOut
	fprintf(stderr," >> MPI_Scan...............\n");
	#endif
	
	before_mpi(Scan, (intptr_t)sendbuf, (intptr_t)recvbuf);
}

__attribute__((alias("DLB_MPI_Scan_leave"))) void DLB_mpi_scan_leave (void);
void DLB_MPI_Scan_leave (void){
	#ifdef debugInOut
	fprintf(stderr," << MPI_Scan...............\n");
	#endif
	
	after_mpi(Scan);
}
