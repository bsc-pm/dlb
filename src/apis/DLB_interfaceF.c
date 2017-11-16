/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#include "apis/dlb.h"
#include "support/debug.h"

#define DLB_API_F_ALIAS( NameF, Params) \
    void NameF##_ Params __attribute__((alias(#NameF))); \
    void NameF##__ Params __attribute__((alias(#NameF)))

#pragma GCC visibility push(default)

#ifdef MPI_LIB
#include <mpi.h>
#include "LB_MPI/process_MPI.h"
void mpi_barrier_(MPI_Comm *, int *) __attribute__((weak));
void dlb_mpi_node_barrier(void)  {
    if (is_mpi_ready()) {
        int ierror;
        MPI_Comm mpi_comm_node = getNodeComm();
        if (mpi_barrier_) {
            mpi_barrier_(&mpi_comm_node, &ierror);
        } else {
            warning("MPI_Barrier symbol could not be found. Please report a ticket.");
        }
    }
}
#else
void dlb_mpi_node_barrier(void)  {}
#endif
DLB_API_F_ALIAS(dlb_mpi_node_barrier, (void));

#pragma GCC visibility pop
