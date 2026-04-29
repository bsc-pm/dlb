/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#ifndef TALP_MPI_H
#define TALP_MPI_H

#include "support/types.h"

typedef struct SubProcessDescriptor subprocess_descriptor_t;

/* TALP MPI functions */
void talp_mpi_init(const subprocess_descriptor_t *spd);
void talp_mpi_finalize(const subprocess_descriptor_t *spd);
void talp_into_sync_call(const subprocess_descriptor_t *spd, sync_call_flags_t flags);
void talp_out_of_sync_call(const subprocess_descriptor_t *spd, sync_call_flags_t flags);

#endif /* TALP_MPI_H */
