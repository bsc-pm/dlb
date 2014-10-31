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
    "MPI_SendRecv"
};
