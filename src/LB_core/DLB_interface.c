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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <limits.h>

#include "LB_core/dlb_types.h"
#include "LB_core/dlb.h"
#include "LB_core/DLB_kernel.h"
#include "support/error.h"

#pragma GCC visibility push(default)

/* Status */

int DLB_Init(const_dlb_cpu_set_t mask, const char *dlb_args) {
    return Initialize(mask, dlb_args);
}

int DLB_Finalize(void) {
    return Finish();
}

int DLB_Enable(void) {
    return set_dlb_enabled(true);
}

int DLB_Disable(void) {
    return set_dlb_enabled(false);
}

int DLB_SetMaxParallelism(int max) {
    return set_max_parallelism(max);
}


/* Callbacks */

int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback) {
    return callback_set(which, callback);
}

int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t callback) {
    return callback_get(which, callback);
}


/* Lend */

int DLB_Lend(void) {
    return lend();
}

int DLB_LendCpu(int cpuid) {
    return lend_cpu(cpuid);
}

int DLB_LendCpus(int ncpus) {
    return lend_cpus(ncpus);
}

int DLB_LendCpuMask(const_dlb_cpu_set_t mask) {
    return lend_cpu_mask(mask);
}


/* Reclaim */

int DLB_Reclaim(void) {
    return reclaim();
}

int DLB_ReclaimCpu(int cpuid) {
    return reclaim_cpu(cpuid);
}

int DLB_ReclaimCpus(int ncpus) {
    return reclaim_cpus(ncpus);
}

int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask) {
    return reclaim_cpu_mask(mask);
}


/* Acquire */

int DLB_Acquire(void) {
    return acquire();
}

int DLB_AcquireCpu(int cpuid) {
    return acquire_cpu(cpuid);
}

int DLB_AcquireCpus(int ncpus) {
    return acquire_cpus(ncpus);
}

int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask) {
    return acquire_cpu_mask(mask);
}


/* Return */

int DLB_Return(void) {
    return return_all();
}

int DLB_ReturnCpu(int cpuid) {
    return return_cpu(cpuid);
}


/* DROM Responsive */

int DLB_PollDROM(int *nthreads, dlb_cpu_set_t mask) {
    return poll_drom(nthreads, mask);
}


/* Misc */

int DLB_CheckCpuAvailability(int cpuid) {
    return checkCpuAvailability(cpuid);
}

int DLB_Barrier(void) {
    return barrier();
}

int DLB_SetVariable(const char *variable, const char *value) {
    return set_variable(variable, value);
}

int DLB_GetVariable(const char *variable, char *value) {
    return get_variable(variable, value);
}

int DLB_PrintVariables(void) {
    return print_variables();
}

int DLB_PrintShmem(void) {
    return print_shmem();
}

const char* DLB_Strerror(int errnum) {
    return error_get_str(errnum);
}


#pragma GCC visibility pop
