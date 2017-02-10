/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#include "apis/dlb_sp.h"

#include "apis/dlb_types.h"
#include "apis/dlb_sp.h"
#include "LB_core/DLB_kernel_sp.h"

#pragma GCC visibility push(default)

/* Status */

dlb_handler_t DLB_Init_sp(const_dlb_cpu_set_t mask, const char *dlb_args) {
    return Initialize_sp(mask, dlb_args);
}

int DLB_Finalize_sp(dlb_handler_t handler) {
    return Finish_sp(handler);
}

int DLB_Enable_sp(dlb_handler_t handler) {
    return set_dlb_enabled_sp(handler, true);
}

int DLB_Disable_sp(dlb_handler_t handler) {
    return set_dlb_enabled_sp(handler, false);
}

int DLB_SetMaxParallelism_sp(dlb_handler_t handler, int max) {
    return set_max_parallelism_sp(handler, max);
}


/* Callbacks */

int DLB_CallbackSet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t callback) {
    return callback_set_sp(handler, which, callback);
}

int DLB_CallbackGet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t *callback) {
    return callback_get_sp(handler, which, callback);
}


/* Lend */

int DLB_Lend_sp(dlb_handler_t handler) {
    return lend_sp(handler);
}

int DLB_LendCpu_sp(dlb_handler_t handler, int cpuid) {
    return lend_cpu_sp(handler, cpuid);
}

int DLB_LendCpus_sp(dlb_handler_t handler, int ncpus) {
    return lend_cpus_sp(handler, ncpus);
}

int DLB_LendCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return lend_cpu_mask_sp(handler, mask);
}


/* Reclaim */

int DLB_Reclaim_sp(dlb_handler_t handler) {
    return reclaim_sp(handler);
}

int DLB_ReclaimCpu_sp(dlb_handler_t handler, int cpuid) {
    return reclaim_cpu_sp(handler, cpuid);
}

int DLB_ReclaimCpus_sp(dlb_handler_t handler, int ncpus) {
    return reclaim_cpus_sp(handler, ncpus);
}

int DLB_ReclaimCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return reclaim_cpu_mask_sp(handler, mask);
}


/* Acquire */

int DLB_Acquire_sp(dlb_handler_t handler) {
    return acquire_sp(handler);
}

int DLB_AcquireCpu_sp(dlb_handler_t handler, int cpuid) {
    return acquire_cpu_sp(handler, cpuid);
}

int DLB_AcquireCpus_sp(dlb_handler_t handler, int ncpus) {
    return acquire_cpus_sp(handler, ncpus);
}

int DLB_AcquireCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return acquire_cpu_mask_sp(handler, mask);
}


/* Return */

int DLB_Return_sp(dlb_handler_t handler) {
    return return_all_sp(handler);
}

int DLB_ReturnCpu_sp(dlb_handler_t handler, int cpuid) {
    return return_cpu_sp(handler, cpuid);
}


/* DROM Responsive */

int DLB_PollDROM_sp(dlb_handler_t handler, int *nthreads, dlb_cpu_set_t mask) {
    return poll_drom_sp(handler, nthreads, mask);
}


/* Misc */

int DLB_SetVariable_sp(dlb_handler_t handler, const char *variable, const char *value) {
    return set_variable_sp(handler, variable, value);
}

int DLB_GetVariable_sp(dlb_handler_t handler, const char *variable, char *value) {
    return get_variable_sp(handler, variable, value);
}

int DLB_PrintVariables_sp(dlb_handler_t handler) {
    return print_variables_sp(handler);
}

#pragma GCC visibility pop
