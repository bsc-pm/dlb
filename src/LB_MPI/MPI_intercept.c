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
#include "config.h"
#endif

#ifdef HAVE_MPI

#include <stdio.h>
#include <mpi.h>
#include <MPI_interface.h>

int whoami;


int MPI_Init (int *argc, char ***argv){

	

	#ifdef debugInOutMPI
	fprintf(stderr," >> MPI_Init...............\n");
	#endif

	int res;
	DLB_MPI_Init_enter (argc, argv);
	res=PMPI_Init(argc, argv);
	DLB_MPI_Init_leave ();
	
	MPI_Comm_rank(MPI_COMM_WORLD,&whoami);

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Init...............\n",whoami);
	#endif

	return res;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
	#ifdef debugInOutMPI
	fprintf(stderr," >> MPI_Init_thread........\n");
	#endif

	int res;
	DLB_MPI_Init_enter (argc, argv);
	res=PMPI_Init_thread(argc, argv, required, provided);
	DLB_MPI_Init_leave ();

	MPI_Comm_rank(MPI_COMM_WORLD,&whoami);

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Init_thread........\n",whoami);
	#endif

	return res;
}

int MPI_Finalize (void){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Finalize...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Finalize_enter ();
	res=PMPI_Finalize();
	DLB_MPI_Finalize_leave ();

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Finalize...............\n",whoami);
	#endif
	
	return res;
}

int MPI_Sendrecv(void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Sendrecv (to %d - from %d)...............\n",whoami, dest, source);
	#endif

	int res;
	DLB_MPI_Sendrecv_enter (sendbuf,sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status);
	res=PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status);
	DLB_MPI_Sendrecv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Sendrecv...............\n",whoami);
	#endif
	
	return res;

}

int MPI_Sendrecv_replace( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status ){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Sendrecv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Sendrecv_replace_enter ( buf, count, datatype, dest, sendtag, source, recvtag, comm, status );
	res=PMPI_Sendrecv_replace( buf, count, datatype, dest, sendtag, source, recvtag, comm, status );
	DLB_MPI_Sendrecv_replace_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Sendrecv...............\n",whoami);
	#endif
	
	return res;

}

int MPI_Bsend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Bsend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Bsend_enter (buf,count, datatype, dest, tag, comm);
	res=PMPI_Bsend(buf, count, datatype, dest, tag, comm);
	DLB_MPI_Bsend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Bsend...............\n",whoami);
	#endif
	
	return res;
}

int MPI_Ssend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Ssend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Ssend_enter (buf, count, datatype, dest, tag, comm);
	res=PMPI_Ssend(buf, count, datatype, dest, tag, comm);
	DLB_MPI_Ssend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Ssend...............\n",whoami);
	#endif
	return res;
}

int MPI_Rsend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Rsend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Rsend_enter (buf, count, datatype, dest, tag, comm);
	res=PMPI_Rsend(buf, count, datatype, dest, tag, comm);
	DLB_MPI_Rsend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Rsend...............\n",whoami);
	#endif
	return res;
}

int MPI_Send (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Send...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Send_enter (buf, count, datatype, dest, tag, comm);
	res=PMPI_Send(buf, count, datatype, dest, tag, comm);
	DLB_MPI_Send_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Send...............\n",whoami);
	#endif
	return res;
}

int MPI_Ibsend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request ){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Ibsend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Ibsend_enter (buf, count, datatype, dest, tag, comm, request );
	res=PMPI_Ibsend(buf, count, datatype, dest, tag, comm, request );
	DLB_MPI_Ibsend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Ibsend...............\n",whoami);
	#endif
	return res;
}

int MPI_Isend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Isend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Isend_enter (buf, count, datatype, dest, tag, comm, request);
	res=PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
	DLB_MPI_Isend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Isend...............\n",whoami);
	#endif
	return res;
}

int MPI_Issend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Issend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Issend_enter (buf, count, datatype, dest, tag, comm, request);
	res=PMPI_Issend(buf, count, datatype, dest, tag, comm, request);
	DLB_MPI_Issend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Issend...............\n",whoami);
	#endif
	return res;
}

int MPI_Irsend (void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Irsend...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Irsend_enter (buf, count, datatype, dest, tag, comm, request);
	res=PMPI_Irsend(buf, count, datatype, dest, tag, comm, request);
	DLB_MPI_Irsend_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Irsend...............\n",whoami);
	#endif
	return res;
}

int MPI_Recv (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Recv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Recv_enter (buf, count, datatype, source, tag, comm, status);
	res=PMPI_Recv(buf, count, datatype, source, tag, comm, status);
	DLB_MPI_Recv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Recv...............\n",whoami);
	#endif
	return res;
}

int MPI_Irecv (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Irecv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Irecv_enter (buf, count, datatype, source, tag, comm, request);
	res=PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
	DLB_MPI_Irecv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Irecv...............\n",whoami);
	#endif
	return res;
}

int MPI_Reduce (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Reduce...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Reduce_enter (sendbuf, recvbuf, count, datatype, op, root, comm);
	res=PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
	DLB_MPI_Reduce_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Reduce...............\n",whoami);
	#endif
	return res;
}

int MPI_Reduce_scatter (void* sendbuf, void* recvbuf, int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Reduce_scatter...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Reduce_scatter_enter (sendbuf, recvbuf, recvcounts, datatype, op, comm);
	res=PMPI_Reduce_scatter(sendbuf, recvbuf, recvcounts, datatype, op, comm);
	DLB_MPI_Reduce_scatter_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Reduce_scatter...............\n",whoami);
	#endif
	return res;
}

int MPI_Allreduce (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Allreduce...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Allreduce_enter (sendbuf, recvbuf, count, datatype, op, comm);
	res=PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
	DLB_MPI_Allreduce_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Allreduce...............\n",whoami);
	#endif
	return res;
}

int MPI_Barrier (MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Barrier...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Barrier_enter (comm);
	res=PMPI_Barrier(comm);
	DLB_MPI_Barrier_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Barrier...............\n",whoami);
	#endif
	return res;
}

int MPI_Wait (MPI_Request  *request, MPI_Status   *status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Wait...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Wait_enter (request, status);
	res=PMPI_Wait(request, status);
	DLB_MPI_Wait_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Wait...............\n",whoami);
	#endif
	return res;
}

int MPI_Waitall (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Waitall...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Waitall_enter (count, array_of_requests, array_of_statuses);
	res=PMPI_Waitall(count, array_of_requests, array_of_statuses);
	DLB_MPI_Waitall_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Waitall...............\n",whoami);
	#endif
	return res;
}

int MPI_Waitany (int count, MPI_Request *requests, int* index, MPI_Status* status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Waitany...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Waitany_enter (count, requests, index, status);
	res=PMPI_Waitany(count, requests, index, status);
	DLB_MPI_Waitany_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Waitany...............\n",whoami);
	#endif
	return res;
}

int MPI_Waitsome (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Waitsome...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Waitsome_enter (incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	res=PMPI_Waitsome(incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	DLB_MPI_Waitsome_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Waitsome...............\n",whoami);
	#endif
	return res;
}

int MPI_Bcast (void* buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Bcast...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Bcast_enter  (buffer, count, datatype, root, comm);
	res=PMPI_Bcast (buffer, count, datatype, root, comm);
	DLB_MPI_Bcast_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Bcast...............\n",whoami);
	#endif
	return res;
}

int MPI_Alltoall (void* sendbuf, int sendcount, MPI_Datatype sendtype, void* recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Alltoall...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Alltoall_enter (sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm);
	res=PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm);
	DLB_MPI_Alltoall_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Alltoall...............\n",whoami);
	#endif
	return res;
}

int MPI_Alltoallv (void* sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void* recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Alltoallv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Alltoallv_enter(sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm);
	res=PMPI_Alltoallv(sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm);
	DLB_MPI_Alltoallv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Alltoallv...............\n",whoami);
	#endif
	return res;
}

int MPI_Allgather (void* sendbuf, int sendcount, MPI_Datatype sendtype, void* recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Allgather...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Allgather_enter (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
	res=PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
	DLB_MPI_Allgather_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Allgather...............\n",whoami);
	#endif
	return res;
}

int MPI_Allgatherv (void* sendbuf, int sendcount, MPI_Datatype sendtype, void* recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Allgatherv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Allgatherv_enter (sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm);
	res=PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm);
	DLB_MPI_Allgatherv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Allgatherv...............\n",whoami);
	#endif
	return res;
}

int MPI_Gather (void* sendbuf, int sendcnt, MPI_Datatype sendtype, void* recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Gather...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Gather_enter (sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm);
	res=PMPI_Gather(sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm);
	DLB_MPI_Gather_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Gather...............\n",whoami);
	#endif
	return res;
}

int MPI_Gatherv (void* sendbuf, int sendcnt, MPI_Datatype sendtype, void* recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm ){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Gatherv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Gatherv_enter (sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm );
	res=PMPI_Gatherv(sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm );
	DLB_MPI_Gatherv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Gatherv...............\n",whoami);
	#endif
	return res;
}

int MPI_Scatter (void* sendbuf, int sendcnt, MPI_Datatype sendtype, void* recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Scatter...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Scatter_enter (sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	res=PMPI_Scatter(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	DLB_MPI_Scatter_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Scatter...............\n",whoami);
	#endif
	return res;
}

int MPI_Scatterv (void* sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void* recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Scatterv...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Scatterv_enter (sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	res=PMPI_Scatterv(sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	DLB_MPI_Scatterv_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Scatterv...............\n",whoami);
	#endif
	return res;
}

int MPI_Scan (void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Scan...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Scan_enter (sendbuf, recvbuf, count, datatype, op, comm);
	res=PMPI_Scan(sendbuf, recvbuf, count, datatype, op, comm);
	DLB_MPI_Scan_leave ();
	
	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Scan...............\n",whoami);
	#endif
	return res;
}

int MPI_Test (MPI_Request *request, int *flag, MPI_Status *status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Test...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Test_enter (request, flag, status);
	res=PMPI_Test (request, flag, status);
	DLB_MPI_Test_leave ();

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Test...............\n",whoami);
	#endif
	return res;
}

int MPI_Testall (int count, MPI_Request array_of_requests[], int *flag, MPI_Status array_of_statuses[]){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Testall...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Testall_enter (count, array_of_requests, flag, array_of_statuses);
	res=PMPI_Testall (count, array_of_requests, flag, array_of_statuses);
	DLB_MPI_Testall_leave ();

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Testall...............\n",whoami);
	#endif
	return res;
}

int MPI_Testany (int count, MPI_Request array_of_requests[], int *indx, int *flag, MPI_Status *status){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Testany...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Testany_enter (count, array_of_requests, indx, flag, status);
	res=PMPI_Testany (count, array_of_requests, indx, flag, status);
	DLB_MPI_Testany_leave ();

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Testany...............\n",whoami);
	#endif
	return res;
}

int MPI_Testsome (int incount, MPI_Request array_of_requests[], int *outcount, int array_of_indices[], MPI_Status array_of_statuses[]){
	#ifdef debugInOutMPI
	fprintf(stderr,"%d >> MPI_Testsome...............\n",whoami);
	#endif

	int res;
	DLB_MPI_Testsome_enter (incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	res=PMPI_Testsome (incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	DLB_MPI_Testsome_leave ();

	#ifdef debugInOutMPI
	fprintf(stderr,"%d << MPI_Testsome...............\n",whoami);
	#endif
	return res;
}

#endif /* HAVE_MPI */
