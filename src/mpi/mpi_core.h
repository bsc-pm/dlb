/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef HANDLE_MPI_H
#define HANDLE_MPI_H

#include "mpi/mpi_calls_coded.h"

#include <unistd.h>
#include <mpi.h>

extern int _mpi_rank;         /* MPI rank */
extern int _mpi_size;         /* MPI size */
extern int _node_id;          /* Node ID */
extern int _num_nodes;        /* Number of nodes */
extern int _process_id;       /* Process ID per node */
extern int _mpis_per_node;    /* Numer of MPI processes per node */

void before_mpi(mpi_call_t mpi_call);
void after_mpi(mpi_call_t mpi_call);
int  is_mpi_ready(void);
void finalize_mpi_core(void);
MPI_Comm getWorldComm(void);
MPI_Comm getNodeComm(void);
MPI_Comm getInterNodeComm(void);
MPI_Datatype get_mpi_int64_type(void);

#endif /* HANDLE_MPI_H */
