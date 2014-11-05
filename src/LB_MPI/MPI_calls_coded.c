/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
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

const char* mpi_call_names[] = {
    "Unknown MPI call",
    "MPI_Bsend",
    "MPI_Ssend",
    "MPI_Rsend",
    "MPI_Send",
    "MPI_Ibsend",
    "MPI_Isend",
    "MPI_Issend",
    "MPI_Irsend",
    "MPI_Recv",
    "MPI_Irecv",
    "MPI_Reduce",
    "MPI_Reduce_scatter",
    "MPI_Allreduce",
    "MPI_Barrier",
    "MPI_Wait",
    "MPI_Waitall",
    "MPI_Waitany",
    "MPI_Waitsome",
    "MPI_Bcast",
    "MPI_Alltoall",
    "MPI_Alltoallv",
    "MPI_Allgather",
    "MPI_Allgatherv",
    "MPI_Gather",
    "MPI_Gatherv",
    "MPI_Scatter",
    "MPI_Scatterv",
    "MPI_Scan",
    "MPI_Sendrecv",
    "MPI_Sendrecv_replace",
    "MPI_Test",
    "MPI_Testall",
    "MPI_Testany",
    "MPI_Testsome",
    "MPI_Probe",
    "MPI_IProbe",
    "MPI_Request_get_status",
    "MPI_Request_free",
    "MPI_Cancel",
    "MPI_Comm_Rank",
    "MPI_Comm_Size",
    "MPI_Comm_Create",
    "MPI_Comm_Free",
    "MPI_Comm_Dup",
    "MPI_Comm_Split",
    "MPI_Comm_spawn",
    "MPI_Comm_spawn_multiple",
    "MPI_Cart_create",
    "MPI_Cart_sub",
    "MPI_Start",
    "MPI_Startall",
    "MPI_Recv_init",
    "MPI_Send_init",
    "MPI_Bsend_init",
    "MPI_Rsend_init",
    "MPI_Ssend_init",
    "MPI_File_close",
    "MPI_File_read",
    "MPI_File_read_all",
    "MPI_File_write",
    "MPI_File_write_all",
    "MPI_File_read_at",
    "MPI_File_read_at_all",
    "MPI_file_write_at",
    "MPI_File_write_at_all",
    "MPI_Get",
    "MPI_Put",
    "Win_create",
    "Win_free",
    "Win_fence",
    "Win_start",
    "Win_complete",
    "Win_wait"
};
