/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#ifndef NODE_BARRIER_H
#define NODE_BARRIER_H

#include "LB_core/spd.h"
#include "LB_comm/shmem_barrier.h"

void node_barrier_init(subprocess_descriptor_t *spd);
void node_barrier_finalize(subprocess_descriptor_t *spd);
barrier_t* node_barrier_register(subprocess_descriptor_t *spd,
        const char *barrier_name, int flags);
int node_barrier(const subprocess_descriptor_t *spd, barrier_t *barrier);
int node_barrier_attach(subprocess_descriptor_t *spd, barrier_t *barrier);
int node_barrier_detach(subprocess_descriptor_t *spd, barrier_t *barrier);

#endif /* NODE_BARRIER_H */
