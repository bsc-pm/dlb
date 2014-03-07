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

#ifndef MPI_INTERFACE_H
#define MPI_INTERFACE_H

#include <mpi.h>

#if MPI_VERSION >= 3
#define MPI3_CONST const
#else
#define MPI3_CONST
#endif

void DLB_MPI_Init_enter (int *argc, char ***argv);

void DLB_MPI_Init_leave (void);

void DLB_MPI_Finalize_enter (void);

void DLB_MPI_Finalize_leave (void);

void DLB_MPI_Bsend_enter (MPI3_CONST void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Bsend_leave (void);

void DLB_MPI_Ssend_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Ssend_leave (void);

void DLB_MPI_Rsend_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Rsend_leave (void);

void DLB_MPI_Send_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

void DLB_MPI_Send_leave (void);

void DLB_MPI_Ibsend_enter (MPI3_CONST void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request );

void DLB_MPI_Ibsend_leave (void);

void DLB_MPI_Isend_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Isend_leave (void);

void DLB_MPI_Issend_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Issend_leave (void);

void DLB_MPI_Irsend_enter (MPI3_CONST void*buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Irsend_leave (void);

void DLB_MPI_Recv_enter (void*buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);

void DLB_MPI_Recv_leave (void);

void DLB_MPI_Irecv_enter (void* buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request);

void DLB_MPI_Irecv_leave (void);

void DLB_MPI_Reduce_enter (MPI3_CONST void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm);

void DLB_MPI_Reduce_leave (void);
								
void DLB_MPI_Reduce_scatter_enter (MPI3_CONST void* sendbuf, void* recvbuf, MPI3_CONST int* recvcounts, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Reduce_scatter_leave (void);

void DLB_MPI_Allreduce_enter (MPI3_CONST void* sendbuf, void* recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Allreduce_leave (void);

void DLB_MPI_Barrier_enter (MPI_Comm comm);

void DLB_MPI_Barrier_leave (void);

void DLB_MPI_Wait_enter (MPI_Request  *request, MPI_Status   *status);

void DLB_MPI_Wait_leave (void);

void DLB_MPI_Waitall_enter (int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses);

void DLB_MPI_Waitall_leave (void);

void DLB_MPI_Waitany_enter (int count, MPI_Request *requests, int* index, MPI_Status* status);

void DLB_MPI_Waitany_leave (void);

void DLB_MPI_Waitsome_enter (int incount,MPI_Request *array_of_requests, int *outcount,int *array_of_indices,MPI_Status *array_of_statuses);

void DLB_MPI_Waitsome_leave (void);

void DLB_MPI_Bcast_enter (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);

void DLB_MPI_Bcast_leave (void);

void DLB_MPI_Alltoall_enter (MPI3_CONST void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Alltoall_leave (void);

void DLB_MPI_Alltoallv_enter (MPI3_CONST void *sendbuf, MPI3_CONST int *sendcnts, MPI3_CONST int *sdispls, MPI_Datatype sendtype, void *recvbuf, MPI3_CONST int *recvcnts, MPI3_CONST int *rdispls, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Alltoallv_leave (void);


void DLB_MPI_Allgather_enter (MPI3_CONST void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Allgather_leave (void);

void DLB_MPI_Allgatherv_enter (MPI3_CONST void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, MPI3_CONST int *recvcounts, MPI3_CONST int *displs, MPI_Datatype recvtype, MPI_Comm comm);

void DLB_MPI_Allgatherv_leave (void);

void DLB_MPI_Gather_enter (MPI3_CONST void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Gather_leave (void);
 
void DLB_MPI_Gatherv_enter (MPI3_CONST void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, MPI3_CONST int *recvcnts, MPI3_CONST int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm );

void DLB_MPI_Gatherv_leave (void);

void DLB_MPI_Scatter_enter (MPI3_CONST void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Scatter_leave (void);

void DLB_MPI_Scatterv_enter (MPI3_CONST void *sendbuf, MPI3_CONST int *sendcnts, MPI3_CONST int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm);

void DLB_MPI_Scatterv_leave (void);

void DLB_MPI_Scan_enter (MPI3_CONST void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);

void DLB_MPI_Scan_leave (void);

void DLB_MPI_Sendrecv_replace_enter ( void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status );

void DLB_MPI_Sendrecv_replace_leave (void);

void DLB_MPI_Sendrecv_enter ( MPI3_CONST void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status );

void DLB_MPI_Sendrecv_leave (void);

void  DLB_MPI_Test_enter (MPI_Request *request, int *flag, MPI_Status *status);

void  DLB_MPI_Test_leave (void);

void  DLB_MPI_Testall_enter (int count, MPI_Request array_of_requests[], int *flag, MPI_Status array_of_statuses[]);

void  DLB_MPI_Testall_leave (void);

void  DLB_MPI_Testany_enter (int count, MPI_Request array_of_requests[], int *indx, int *flag, MPI_Status *status);

void  DLB_MPI_Testany_leave (void);

void  DLB_MPI_Testsome_enter (int incount, MPI_Request array_of_requests[], int *outcount, int array_of_indices[], MPI_Status array_of_statuses[]);

void  DLB_MPI_Testsome_leave (void);

#endif //MPI_INTERFACE_H


