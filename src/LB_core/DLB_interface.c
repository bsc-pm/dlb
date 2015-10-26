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

#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include "LB_core/DLB_interface.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/statistics.h"
#include "LB_core/drom.h"
#include "support/dlb_api.h"
#include "support/tracing.h"
#include "support/debug.h"

// sub-process ID
static int spid = 0;

#pragma GCC visibility push(default)

void DLB_Init (void) {
    fatal_cond(spid != 0, "DLB has been already initialized\n");
    spid = Initialize();
}

void DLB_Finalize (void) {
    Finish( spid );
}

void DLB_enable (void) {
    //FIXME This hiddes the single event always
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    set_dlb_enabled( true );
}

void DLB_disable (void) {
    //FIXME This hiddes the single event always
    add_event(DLB_MODE_EVENT, EVENT_DISABLED);
    set_dlb_enabled( false );
}

void DLB_reset (void) {
    add_event(RUNTIME_EVENT, EVENT_RESET);
    resetDLB();
    add_event(RUNTIME_EVENT, 0);
}

void DLB_single (void) {
    add_event(DLB_MODE_EVENT, EVENT_SINGLE);
    singlemode();
}

void DLB_parallel (void) {
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    parallelmode();
}

void DLB_UpdateResources (void) {
    updateresources( USHRT_MAX );
}

void DLB_UpdateResources_max (int max_resources) {
    updateresources( max_resources );
}

void DLB_ReturnClaimedCpus (void) {
    returnclaimed();
}

int DLB_ReleaseCpu (int cpu) {
    return releasecpu(cpu);
}

int DLB_ReturnClaimedCpu (int cpu) {
    return returnclaimedcpu(cpu);
}

void DLB_ClaimCpus (int cpus) {
    claimcpus(cpus);
}

void DLB_Lend (void) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    //no iter, no single
    IntoBlockingCall(0, 0);
    add_event(RUNTIME_EVENT, 0);
}

void DLB_Retrieve (void) {
    add_event(RUNTIME_EVENT, EVENT_RETRIEVE);
    OutOfBlockingCall(0);
    add_event(RUNTIME_EVENT, 0);
}

int DLB_CheckCpuAvailability (int cpu) {
    return checkCpuAvailability(cpu);
}

int DLB_Is_auto (void) {
    return is_auto();
}

void DLB_Update (void) {
    Update();
}

void DLB_AcquireCpu (int cpu) {
    acquirecpu(cpu);
}

void DLB_AcquireCpus(dlb_cpu_set_t mask) {
    acquirecpus(mask);
}

void DLB_PrintShmem(void) {
    printShmem();
}

/* Statistics API */

void DLB_Stats_Init (void) {
    stats_ext_init();
}

void DLB_Stats_Finalize (void) {
    stats_ext_finalize();
}

int DLB_Stats_GetNumCpus (void) {
    return stats_ext_getnumcpus();
}

void DLB_Stats_GetPidList (int *pidlist, int *nelems, int max_len) {
    stats_ext_getpidlist(pidlist, nelems, max_len);
}

double DLB_Stats_GetCpuUsage (int pid) {
    return stats_ext_getcpuusage(pid);
}

void DLB_Stats_GetCpuUsageList (double *usagelist, int *nelems, int max_len) {
    stats_ext_getcpuusage_list(usagelist, nelems, max_len);
}

double DLB_Stats_GetNodeUsage(void) {
    return stats_ext_getnodeusage();
}

int DLB_Stats_GetActiveCpus (int pid) {
    return stats_ext_getactivecpus(pid);
}

void DLB_Stats_GetActiveCpusList (int *cpuslist, int *nelems, int max_len) {
    stats_ext_getactivecpus_list(cpuslist, nelems, max_len);
}

int DLB_Stats_GetLoadAvg (int pid, double *load) {
    return stats_ext_getloadavg(pid, load);
}

float DLB_Stats_GetCpuStateIdle( int cpu ) {
    return stats_ext_getcpustateidle(cpu);
}

float DLB_Stats_GetCpuStateOwned( int cpu ) {
    return stats_ext_getcpustateowned(cpu);
}

float DLB_Stats_GetCpuStateGuested( int cpu ) {
    return stats_ext_getcpustateowned(cpu);
}

void DLB_Stats_PrintShmem(void) {
    stats_ext_printshmem();
}

// final name not decided yet
/* Dynamic Resource Ownership Manager API */

void DLB_Drom_Init (void) {
    drom_ext_init();
}

void DLB_Drom_Finalize (void) {
    drom_ext_finalize();
}

int DLB_Drom_GetNumCpus (void) {
    return drom_ext_getnumcpus();
}

void DLB_Drom_GetPidList (int *pidlist, int *nelems, int max_len) {
    drom_ext_getpidlist(pidlist, nelems, max_len);
}

int DLB_Drom_GetProcessMask (int pid, dlb_cpu_set_t mask) {
    return drom_ext_getprocessmask( pid, (cpu_set_t*)mask);
}

int DLB_Drom_SetProcessMask (int pid, const dlb_cpu_set_t mask) {
    return drom_ext_setprocessmask( pid, (cpu_set_t*)mask);
}

void DLB_Drom_PrintShmem(void) {
    drom_ext_printshmem();
}

/* MPI API */
#ifdef MPI_LIB
#include <mpi.h>
#include "LB_MPI/process_MPI.h"
void DLB_MPI_node_barrier(void) {
    if (is_mpi_ready()) {
        MPI_Comm mpi_comm_node = getNodeComm();
        MPI_Barrier(mpi_comm_node);
    }
}
#else
void DLB_MPI_node_barrier(void) {}
#endif

#pragma GCC visibility pop
