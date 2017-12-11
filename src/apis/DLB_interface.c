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

#include "apis/DLB_interface.h"

#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "support/error.h"

#include <unistd.h>

/* Global sub-process descriptor */
static subprocess_descriptor_t spd = { 0 };

/* Private functions not exposed to the API */

const subprocess_descriptor_t* get_global_spd(void) {
    if (!spd.dlb_initialized) return NULL;
    return &spd;
}

const options_t* get_global_options(void) {
    if (!spd.dlb_initialized) return NULL;
    return &spd.options;
}

#pragma GCC visibility push(default)

/* Status */

int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args) {
    if (__sync_bool_compare_and_swap(&spd.dlb_initialized, false, true)) {
        return Initialize(&spd, getpid(), ncpus, mask, dlb_args);
    } else {
        return DLB_ERR_INIT;
    }
}

int DLB_Finalize(void) {
    if (__sync_bool_compare_and_swap(&spd.dlb_initialized, true, false)) {
        return Finish(&spd);
    } else {
        return DLB_ERR_NOINIT;
    }
}

int DLB_Enable(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return set_dlb_enabled(&spd, true);
}

int DLB_Disable(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return set_dlb_enabled(&spd, false);
}

int DLB_SetMaxParallelism(int max) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return set_max_parallelism(&spd, max);
}


/* Callbacks */

int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg) {
    return pm_callback_set(&spd.pm, which, callback, arg);
}

int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg) {
    return pm_callback_get(&spd.pm, which, callback, arg);
}


/* Lend */

int DLB_Lend(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return lend(&spd);
}

int DLB_LendCpu(int cpuid) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return lend_cpu(&spd, cpuid);
}

int DLB_LendCpus(int ncpus) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return lend_cpus(&spd, ncpus);
}

int DLB_LendCpuMask(const_dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return lend_cpu_mask(&spd, mask);
}


/* Reclaim */

int DLB_Reclaim(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return reclaim(&spd);
}

int DLB_ReclaimCpu(int cpuid) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return reclaim_cpu(&spd, cpuid);
}

int DLB_ReclaimCpus(int ncpus) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return reclaim_cpus(&spd, ncpus);
}

int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return reclaim_cpu_mask(&spd, mask);
}


/* Acquire */

int DLB_AcquireCpu(int cpuid) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return acquire_cpu(&spd, cpuid);
}

int DLB_AcquireCpus(int ncpus) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return acquire_cpus(&spd, ncpus);
}

int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return acquire_cpu_mask(&spd, mask);
}


/* Borrow */

int DLB_Borrow(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return borrow(&spd);
}

int DLB_BorrowCpu(int cpuid) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return borrow_cpu(&spd, cpuid);
}

int DLB_BorrowCpus(int ncpus) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return borrow_cpus(&spd, ncpus);
}

int DLB_BorrowCpuMask(const_dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return borrow_cpu_mask(&spd, mask);
}


/* Return */

int DLB_Return(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return return_all(&spd);
}

int DLB_ReturnCpu(int cpuid) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return return_cpu(&spd, cpuid);
}

int DLB_ReturnCpuMask(const_dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return return_cpu_mask(&spd, mask);
}


/* DROM Responsive */

int DLB_PollDROM(int *ncpus, dlb_cpu_set_t mask) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return poll_drom(&spd, ncpus, mask);
}

int DLB_PollDROM_Update(void) {
    if (!spd.dlb_initialized) return DLB_ERR_NOINIT;
    return poll_drom_update(&spd);
}


/* Misc */

int DLB_CheckCpuAvailability(int cpuid) {
    return check_cpu_availability(&spd, cpuid);
}

int DLB_Barrier(void) {
    return node_barrier();
}

int DLB_SetVariable(const char *variable, const char *value) {
    return options_set_variable(&spd.options, variable, value);
}

int DLB_GetVariable(const char *variable, char *value) {
    return options_get_variable(&spd.options, variable, value);
}

int DLB_PrintVariables(int print_extra) {
    if (!print_extra) {
        options_print_variables(&spd.options);
    } else {
        options_print_variables_extra(&spd.options);
    }
    return DLB_SUCCESS;
}

int DLB_PrintShmem(int num_columns, dlb_printshmem_flags_t print_flags) {
    return print_shmem(&spd, num_columns, print_flags);
}

const char* DLB_Strerror(int errnum) {
    return error_get_str(errnum);
}


#pragma GCC visibility pop
