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
#include "support/tracing.h"
#include "support/options.h"
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
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    set_dlb_enabled(true);
    return DLB_SUCCESS;
}

int DLB_Disable(void) {
    add_event(DLB_MODE_EVENT, EVENT_DISABLED);
    set_dlb_enabled(false);
    return DLB_SUCCESS;
}

int DLB_SetMaxParallelism(int max) {
    return DLB_SUCCESS;
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
    add_event(RUNTIME_EVENT, EVENT_LEND);
    //no iter, no single
    IntoBlockingCall(0, 0);
    add_event(RUNTIME_EVENT, 0);
    return DLB_SUCCESS;
}

int DLB_LendCpu(int cpuid) {
    return releasecpu(cpuid);
}

int DLB_LendCpus(int ncpus) {
    return DLB_SUCCESS;
}

int DLB_LendCpuMask(const_dlb_cpu_set_t mask) {
    return DLB_SUCCESS;
}


/* Reclaim */

int DLB_Reclaim(void) {
    add_event(RUNTIME_EVENT, EVENT_RETRIEVE);
    OutOfBlockingCall(0);
    add_event(RUNTIME_EVENT, 0);
    return DLB_SUCCESS;
}

int DLB_ReclaimCpu(int cpuid) {
    return DLB_SUCCESS;
}

int DLB_ReclaimCpus(int ncpus) {
    claimcpus(ncpus);
    return DLB_SUCCESS;
}

int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask) {
    return DLB_SUCCESS;
}


/* Acquire */

int DLB_Acquire(void) {
    updateresources(USHRT_MAX);
    return DLB_SUCCESS;
}

int DLB_AcquireCpu(int cpuid) {
    acquirecpu(cpuid);
    return DLB_SUCCESS;
}

int DLB_AcquireCpus(int ncpus) {
    updateresources(ncpus);
    return DLB_SUCCESS;
}

int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask) {
    acquirecpus(mask);
    return DLB_SUCCESS;
}


/* Return */

int DLB_Return(void) {
    returnclaimed();
    return DLB_SUCCESS;
}

int DLB_ReturnCpu(int cpuid) {
    return returnclaimedcpu(cpuid);
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
    add_event(RUNTIME_EVENT, EVENT_BARRIER);
    nodebarrier();
    add_event(RUNTIME_EVENT, 0);
    return DLB_SUCCESS;
}

int DLB_SetVariable(const char *variable, const char *value) {
    return options_set_variable(&global_spd.options, variable, value);
}

int DLB_GetVariable(const char *variable, char *value) {
    return options_set_variable(&global_spd.options, variable, value);
}

int DLB_PrintVariables(void) {
    options_print_variables(&global_spd.options);
    return DLB_SUCCESS;
}

int DLB_PrintShmem(void) {
    printShmem();
    return DLB_SUCCESS;
}

const char* DLB_Strerror(int errnum) {
    return error_get_str(errnum);
}


#pragma GCC visibility pop
