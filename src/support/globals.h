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

#ifndef GLOBALS_H
#define GLOBALS_H

#include "utils.h"

extern int _mpi_rank;         /* MPI rank */
extern int _mpi_size;         /* MPI size */
extern int _node_id;          /* Node ID */
extern int _process_id;       /* Process ID per node */
extern int _mpis_per_node;    /* Numer of MPI processes per node */
extern int _default_nthreads; /* Number of threads per MPI process */

extern bool _locality_aware;
extern bool _just_barrier;
extern blocking_mode_t _blocking_mode;

#endif /* GLOBALS_H */
