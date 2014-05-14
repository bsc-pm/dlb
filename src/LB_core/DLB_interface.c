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

#include <limits.h>
#include "LB_core/DLB_interface.h"
#include "LB_core/DLB_kernel.h"
#include "LB_MPI/process_MPI.h"
#include "support/utils.h"
#include "support/tracing.h"

static bool dlb_enabled = true;

DLB_API_DEF( void, DLB_Init, dlb_init, (void) )
{
   fixme_init_without_mpi();
   dlb_enabled = true;
}

DLB_API_DEF( void, DLB_Finalize, dlb_finalize, (void) )
{
   dlb_enabled = false;
   fixme_finalize_without_mpi();
}

DLB_API_DEF( void, DLB_enable, dlb_enable, (void) )
{
   add_event(DLB_MODE_EVENT, EVENT_ENABLED);
   enable_mpi();
   dlb_enabled = true;
}

DLB_API_DEF( void, DLB_disable, dlb_disable, (void) )
{
   //FIXME This hiddes the single event always
   add_event(DLB_MODE_EVENT, EVENT_DISABLED);
   dlb_enabled = false;
   disable_mpi();
}

DLB_API_DEF( void, DLB_single, dlb_single, (void) )
{
   if ( dlb_enabled ) {
      add_event(DLB_MODE_EVENT, EVENT_SINGLE);
      //no iter, single
      IntoBlockingCall(0, 1);
   }
}

DLB_API_DEF( void, DLB_parallel, dlb_parallel, (void) )
{
   if ( dlb_enabled ) {
      add_event(DLB_MODE_EVENT, EVENT_ENABLED);
      OutOfBlockingCall(0);
   }
}

DLB_API_DEF( void, DLB_UpdateResources, dlb_updateresources, (void) )
{
   if ( dlb_enabled )
      updateresources( USHRT_MAX );
}

DLB_API_DEF( void, DLB_UpdateResources_max, dlb_updateresources_max, (int max_resources) )
{
   if ( dlb_enabled )
      updateresources( max_resources );
}

DLB_API_DEF( void, DLB_ReturnClaimedCpus, dlb_returnclaimedcpus, (void) )
{
   if ( dlb_enabled )
      returnclaimed();
}

DLB_API_DEF( int, DLB_ReleaseCpu, dlb_releasecpu, (int cpu) )
{
   if ( dlb_enabled )
      return releasecpu(cpu);
   else
      return 0;
}

DLB_API_DEF( int, DLB_ReturnClaimedCpu, dlb_returnclaimedcpu, (int cpu) )
{
   if ( dlb_enabled )
      return returnclaimedcpu(cpu);
   else
      return 0;
}

DLB_API_DEF( void, DLB_ClaimCpus, dlb_claimcpus, (int cpus) )
{
   if ( dlb_enabled )
      claimcpus(cpus);
}

DLB_API_DEF( void, DLB_Lend, dlb_lend, (void) )
{
   if ( dlb_enabled )
	//no iter, no single
      IntoBlockingCall(0, 0);
}

DLB_API_DEF( void, DLB_Retrieve, dlb_retrieve, (void) )
{
   if ( dlb_enabled )
      OutOfBlockingCall(0);
}

DLB_API_DEF( void, DLB_Barrier, dlb_barrier, (void) )
{
   if ( dlb_enabled )
      node_barrier();
}

DLB_API_DEF( int, DLB_CheckCpuAvailability, dlb_checkcpuavailability, (int cpu) )
{
   if ( dlb_enabled )
      return checkCpuAvailability(cpu);
   else
      return 1;

}

