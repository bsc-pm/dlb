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

#include "LB_core/DLB_interface.h"
#include "support/dlb_api.h"
#include "support/debug.h"

#pragma GCC visibility push(default)

void dlb_init(void) {
    DLB_Init(NULL, NULL);
}
DLB_API_F_ALIAS(dlb_init, (void));

void dlb_finalize(void) {
    DLB_Finalize();
}
DLB_API_F_ALIAS(dlb_finalize, (void));

void dlb_enable(void) {
    DLB_enable();
}
DLB_API_F_ALIAS(dlb_enable, (void));

void dlb_disable(void) {
    DLB_disable();
}
DLB_API_F_ALIAS(dlb_disable, (void));

void dlb_reset(void) {
    DLB_reset();
}
DLB_API_F_ALIAS(dlb_reset, (void));

void dlb_single(void) {
    DLB_single();
}
DLB_API_F_ALIAS(dlb_single, (void));

void dlb_parallel(void) {
    DLB_parallel();
}
DLB_API_F_ALIAS(dlb_parallel, (void));

void dlb_updateresources(void) {
    DLB_UpdateResources();
}
DLB_API_F_ALIAS(dlb_updateresources, (void));

void dlb_updateresources_max(int *max_resources) {
    DLB_UpdateResources_max(*max_resources);
}
DLB_API_F_ALIAS(dlb_updateresources_max, (int*));

void  dlb_returnclaimedcpus(void) {
    DLB_ReturnClaimedCpus();
}
DLB_API_F_ALIAS(dlb_returnclaimedcpus, (void));

void dlb_releasecpu(int *cpu, int *success) {
    *success = DLB_ReleaseCpu(*cpu);
}
DLB_API_F_ALIAS(dlb_releasecpu, (int*, int*));

void dlb_returnclaimedcpu(int *cpu, int *success) {
    *success = DLB_ReturnClaimedCpu(*cpu);
}
DLB_API_F_ALIAS(dlb_returnclaimedcpu, (int*, int*));

void dlb_claimcpus(int *cpus) {
    DLB_ClaimCpus(*cpus);
}
DLB_API_F_ALIAS(dlb_claimcpus, (int*));

void dlb_lend(void) {
    DLB_Lend();
}
DLB_API_F_ALIAS(dlb_lend, (void));

void dlb_retrieve(void) {
    DLB_Retrieve();
}
DLB_API_F_ALIAS(dlb_retrieve, (void));

void dlb_checkcpuavailability(int *cpu, int *success) {
    *success = DLB_CheckCpuAvailability(*cpu);
}
DLB_API_F_ALIAS(dlb_checkcpuavailability, (int*, int*));

void dlb_update(void) {
    DLB_Update();
}
DLB_API_F_ALIAS(dlb_update, (void));

void dlb_acquirecpu(int *cpu) {
    DLB_AcquireCpu(*cpu);
}
DLB_API_F_ALIAS(dlb_acquirecpu, (int*));

void dlb_barrier(void) {
    DLB_Barrier();
}
DLB_API_F_ALIAS(dlb_barrier, (void));

void dlb_notifyprocessmaskchange(void) {
    DLB_NotifyProcessMaskChange();
}
DLB_API_F_ALIAS(dlb_notifyprocessmaskchange, (void));

void dlb_notifyprocessmaskchangeto(const dlb_cpu_set_t mask) {
    fatal0("Cpu mask functions not implemented in Fortran");
}
DLB_API_F_ALIAS(dlb_notifyprocessmaskchangeto, (dlb_cpu_set_t));

/* Statistics API */

// Termporarily disable Fortran inrterface
#if 0
void dlb_stats_init(void) {
    DLB_Stats_Init();
}
DLB_API_F_ALIAS(dlb_stats_init, (void));

void dlb_stats_finalize(void) {
    DLB_Stats_Finalize();
}
DLB_API_F_ALIAS(dlb_stats_finalize, (void));

void dlb_stats_getnumcpus(int *cpus) {
    *cpus = DLB_Stats_GetNumCpus();
}
DLB_API_F_ALIAS(dlb_stats_getnumcpus, (int*));

void dlb_stats_getpidlist(int *pidlist, int *nelems, int *max_len) {
    DLB_Stats_GetPidList(pidlist, nelems, *max_len);
}
DLB_API_F_ALIAS(dlb_stats_getpidlist, (int*, int*, int*));

void dlb_stats_getcpuusage(int *pid, double *usage) {
    *usage = DLB_Stats_GetCpuUsage(*pid);
}
DLB_API_F_ALIAS(dlb_stats_getcpuusage, (int*, double*));

void dlb_stats_getcpuavgusage(int *pid, double *usage) {
    *usage = DLB_Stats_GetCpuAvgUsage(*pid);
}
DLB_API_F_ALIAS(dlb_stats_getcpuavgusage, (int*, double*));

void dlb_stats_getcpuusagelist(double *usagelist,int *nelems,int *max_len) {
    DLB_Stats_GetCpuUsageList(usagelist, nelems, *max_len);
}
DLB_API_F_ALIAS(dlb_stats_getcpuusagelist, (double*, int*, int*));

void dlb_stats_getcpuavgusagelist(double *avgusagelist,int *nelems,int *max_len) {
    DLB_Stats_GetCpuAvgUsageList(avgusagelist, nelems, *max_len);
}
DLB_API_F_ALIAS(dlb_stats_getcpuavgusagelist, (double*, int*, int*));

void dlb_stats_getnodeusage(double *usage) {
    *usage = DLB_Stats_GetNodeUsage();
}
DLB_API_F_ALIAS(dlb_stats_getnodeusage, (double*));

void dlb_stats_getnodeavgusage(double *usage) {
    *usage = DLB_Stats_GetNodeAvgUsage();
}
DLB_API_F_ALIAS(dlb_stats_getnodeavgusage, (double*));

void dlb_stats_getactivecpus(int *pid, int *cpus) {
    *cpus = DLB_Stats_GetActiveCpus(*pid);
}
DLB_API_F_ALIAS(dlb_stats_getactivecpus, (int*, int*));

void dlb_stats_getactivecpus_list(int *cpuslist, int *nelems, int *max_len) {
    DLB_Stats_GetActiveCpusList(cpuslist, nelems, *max_len);
}
DLB_API_F_ALIAS(dlb_stats_getactivecpus_list, (int*, int*, int*));

void dlb_stats_getloadavg(int *pid, double *load) {
    DLB_Stats_GetLoadAvg(*pid, load);
}
DLB_API_F_ALIAS(dlb_stats_getloadavg, (int*, double*));
#endif

// final name not decided yet
/* Dynamic Resource Ownership Manager API */


// Termporarily disable Fortran inrterface
#if 0
void dlb_drom_init(void) {
    DLB_Drom_Init();
}
DLB_API_F_ALIAS(dlb_drom_init, (void));

void dlb_drom_finalize(void) {
    DLB_Drom_Finalize();
}
DLB_API_F_ALIAS(dlb_drom_finalize, (void));

void dlb_drom_getnumcpus(int *cpus) {
    *cpus = DLB_Drom_GetNumCpus();
}
DLB_API_F_ALIAS(dlb_drom_getnumcpus, (int*));

void dlb_drom_getpidlist(int *pidlist, int *nelems, int *max_len) {
    DLB_Drom_GetPidList(pidlist, nelems, *max_len);
}
DLB_API_F_ALIAS(dlb_drom_getpidlist, (int*, int*, int*));

void dlb_drom_getprocessmask(int *pid, dlb_cpu_set_t mask) {
    fatal0("Cpu mask functions not implemented in Fortran");
}
DLB_API_F_ALIAS(dlb_drom_getprocessmask, (int*, dlb_cpu_set_t));

void dlb_drom_setprocessmask(int *pid, const dlb_cpu_set_t mask) {
    fatal0("Cpu mask functions not implemented in Fortran");
}
DLB_API_F_ALIAS(dlb_drom_setprocessmask, (int*, dlb_cpu_set_t));
#endif

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
