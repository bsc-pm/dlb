/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#include "support/dlb_common.h"

#ifdef MPI_LIB
#include <mpi.h>
#include "LB_MPI/process_MPI.h"
void mpi_barrier_(MPI_Comm *, int *) __attribute__((weak));

DLB_EXPORT_SYMBOL
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
DLB_EXPORT_SYMBOL
void dlb_mpi_node_barrier(void) {}
#endif

DLB_EXPORT_SYMBOL
DLB_ALIAS(void, dlb_mpi_node_barrier_, (void), (), dlb_mpi_node_barrier)

DLB_EXPORT_SYMBOL
DLB_ALIAS(void, dlb_mpi_node_barrier__, (void), (), dlb_mpi_node_barrier)
