/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#include "support/dlb_api.h"

DLB_API_DEF( void, DLB_Init, dlb_init, (void) ) {}
DLB_API_DEF( void, DLB_Finalize, dlb_finalize, (void) ) {}
DLB_API_DEF( void, DLB_enable, dlb_enable, (void) ) {}
DLB_API_DEF( void, DLB_disable, dlb_disable, (void) ) {}
DLB_API_DEF( void, DLB_reset, dlb_reset, (void) ) {}
DLB_API_DEF( void, DLB_single, dlb_single, (void) ) {}
DLB_API_DEF( void, DLB_parallel, dlb_parallel, (void) ) {}
DLB_API_DEF( void, DLB_UpdateResources, dlb_updateresources, (void) ) {}
DLB_API_DEF( void, DLB_UpdateResources_max, dlb_updateresources_max, (int max_resources) ) {}
DLB_API_DEF( void, DLB_ReturnClaimedCpus, dlb_returnclaimedcpus, (void) ) {}
DLB_API_DEF( void, DLB_ReleaseCpu, dlb_releasecpu, (void) ) {}
DLB_API_DEF( void, DLB_ReturnClaimedCpu, dlb_returnclaimedcpu, (void) ) {}
DLB_API_DEF( void, DLB_ClaimCpus, dlb_claimcpus, (void) ) {}
DLB_API_DEF( void, DLB_Lend, dlb_lend, (void) ) {}
DLB_API_DEF( void, DLB_Retrieve, dlb_retrieve, (void) ) {}
DLB_API_DEF( void, DLB_Update, dlb_update, (void) ) {}
DLB_API_DEF( void, DLB_AcquireCpu, dlb_acquirecpu, (void) ) {}

DLB_API_DEF( void, DLB_MPI_node_barrier, dlb_mpi_node_barrier, (void) ) {}
