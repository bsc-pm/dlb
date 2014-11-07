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

#include <limits.h>
#include "LB_core/DLB_kernel.h"
#include "support/dlb_api.h"
#include "support/tracing.h"

// sub-process ID
static int spid = 0;

DLB_API_DEF( void, DLB_Init, dlb_init, (void) ) {
    spid = Initialize();
}

DLB_API_DEF( void, DLB_Finalize, dlb_finalize, (void) ) {
    Finish( spid );
}

DLB_API_DEF( void, DLB_enable, dlb_enable, (void) ) {
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    set_dlb_enabled( true );
}

DLB_API_DEF( void, DLB_disable, dlb_disable, (void) ) {
    //FIXME This hiddes the single event always
    add_event(DLB_MODE_EVENT, EVENT_DISABLED);
    set_dlb_enabled( false );
}

DLB_API_DEF( void, DLB_single, dlb_single, (void) ) {
    add_event(DLB_MODE_EVENT, EVENT_SINGLE);
    //no iter, single
    IntoBlockingCall(0, 1);
}

DLB_API_DEF( void, DLB_parallel, dlb_parallel, (void) ) {
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    OutOfBlockingCall(0);
}

DLB_API_DEF( void, DLB_UpdateResources, dlb_updateresources, (void) ) {
    updateresources( USHRT_MAX );
}

DLB_API_DEF( void, DLB_UpdateResources_max, dlb_updateresources_max, (int max_resources) ) {
    updateresources( max_resources );
}

DLB_API_DEF( void, DLB_ReturnClaimedCpus, dlb_returnclaimedcpus, (void) ) {
    returnclaimed();
}

DLB_API_DEF( int, DLB_ReleaseCpu, dlb_releasecpu, (int cpu) ) {
    return releasecpu(cpu);
}

DLB_API_DEF( int, DLB_ReturnClaimedCpu, dlb_returnclaimedcpu, (int cpu) ) {
    return returnclaimedcpu(cpu);
}

DLB_API_DEF( void, DLB_ClaimCpus, dlb_claimcpus, (int cpus) ) {
    claimcpus(cpus);
}

DLB_API_DEF( void, DLB_Lend, dlb_lend, (void) ) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    //no iter, no single
    IntoBlockingCall(0, 0);
    add_event(RUNTIME_EVENT, 0);
}

DLB_API_DEF( void, DLB_Retrieve, dlb_retrieve, (void) ) {
    add_event(RUNTIME_EVENT, EVENT_RETRIEVE);
    OutOfBlockingCall(0);
    add_event(RUNTIME_EVENT, 0);
}

DLB_API_DEF( int, DLB_CheckCpuAvailability, dlb_checkcpuavailability, (int cpu) ) {
    return checkCpuAvailability(cpu);
}
