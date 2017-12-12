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

#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"

#include <stdlib.h>
#include <unistd.h>

static int spid_seed = 0;
static int get_atomic_spid(void) {
    // pidmax is usually 32k, spid is concatenated in order to keep the pid in the first 5 digits
    const int increment = 100000;
#ifdef HAVE_STDATOMIC_H
    return __atomic_add_fetch(&spid_seed, increment, __ATOMIC_RELAXED) + getpid();
#else
    return __sync_add_and_fetch(&spid_seed, increment) + getpid();
#endif
}


#pragma GCC visibility push(default)

/* Status */

dlb_handler_t DLB_Init_sp(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args) {
    subprocess_descriptor_t *spd = malloc(sizeof(subprocess_descriptor_t));
    spd->dlb_initialized = true;
    int error = Initialize(spd, get_atomic_spid(), ncpus, mask, dlb_args);
    return error == DLB_SUCCESS ? spd : NULL;
}

int DLB_Finalize_sp(dlb_handler_t handler) {
    int error = Finish(handler);
    free(handler);
    return error;
}

int DLB_Enable_sp(dlb_handler_t handler) {
    return set_dlb_enabled(handler, true);
}

int DLB_Disable_sp(dlb_handler_t handler) {
    return set_dlb_enabled(handler, false);
}

int DLB_SetMaxParallelism_sp(dlb_handler_t handler, int max) {
    return set_max_parallelism(handler, max);
}


/* Callbacks */

int DLB_CallbackSet_sp(dlb_handler_t handler, dlb_callbacks_t which,
        dlb_callback_t callback, void *arg) {
    pm_interface_t *pm = &((subprocess_descriptor_t*)handler)->pm;
    return pm_callback_set(pm, which, callback, arg);
}

int DLB_CallbackGet_sp(dlb_handler_t handler, dlb_callbacks_t which,
        dlb_callback_t *callback, void **arg) {
    const pm_interface_t *pm = &((subprocess_descriptor_t*)handler)->pm;
    return pm_callback_get(pm, which, callback, arg);
}


/* Lend */

int DLB_Lend_sp(dlb_handler_t handler) {
    return lend(handler);
}

int DLB_LendCpu_sp(dlb_handler_t handler, int cpuid) {
    return lend_cpu(handler, cpuid);
}

int DLB_LendCpus_sp(dlb_handler_t handler, int ncpus) {
    return lend_cpus(handler, ncpus);
}

int DLB_LendCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return lend_cpu_mask(handler, mask);
}


/* Reclaim */

int DLB_Reclaim_sp(dlb_handler_t handler) {
    return reclaim(handler);
}

int DLB_ReclaimCpu_sp(dlb_handler_t handler, int cpuid) {
    return reclaim_cpu(handler, cpuid);
}

int DLB_ReclaimCpus_sp(dlb_handler_t handler, int ncpus) {
    return reclaim_cpus(handler, ncpus);
}

int DLB_ReclaimCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return reclaim_cpu_mask(handler, mask);
}


/* Acquire */

int DLB_AcquireCpu_sp(dlb_handler_t handler, int cpuid) {
    return acquire_cpu(handler, cpuid);
}

int DLB_AcquireCpus_sp(dlb_handler_t handler, int ncpus) {
    return acquire_cpus(handler, ncpus);
}

int DLB_AcquireCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return acquire_cpu_mask(handler, mask);
}


/* Borrow */

int DLB_Borrow_sp(dlb_handler_t handler) {
    return borrow(handler);
}

int DLB_BorrowCpu_sp(dlb_handler_t handler, int cpuid) {
    return borrow_cpu(handler, cpuid);
}

int DLB_BorrowCpus_sp(dlb_handler_t handler, int ncpus) {
    return borrow_cpus(handler, ncpus);
}

int DLB_BorrowCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return borrow_cpu_mask(handler, mask);
}


/* Return */

int DLB_Return_sp(dlb_handler_t handler) {
    return return_all(handler);
}

int DLB_ReturnCpu_sp(dlb_handler_t handler, int cpuid) {
    return return_cpu(handler, cpuid);
}

int DLB_ReturnCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask) {
    return return_cpu_mask(handler, mask);
}


/* DROM Responsive */

int DLB_PollDROM_sp(dlb_handler_t handler, int *ncpus, dlb_cpu_set_t mask) {
    return poll_drom(handler, ncpus, mask);
}

int DLB_PollDROM_Update_sp(dlb_handler_t handler) {
    return poll_drom_update(handler);
}


/* Misc */

int DLB_CheckCpuAvailability_sp(dlb_handler_t handler, int cpuid) {
    return check_cpu_availability(handler, cpuid);
}

int DLB_SetVariable_sp(dlb_handler_t handler, const char *variable, const char *value) {
    options_t *options = &((subprocess_descriptor_t*)handler)->options;
    return options_set_variable(options, variable, value);
}

int DLB_GetVariable_sp(dlb_handler_t handler, const char *variable, char *value) {
    const options_t *options = &((subprocess_descriptor_t*)handler)->options;
    return options_get_variable(options, variable, value);
}

int DLB_PrintVariables_sp(dlb_handler_t handler, int print_extra) {
    const options_t *options = &((subprocess_descriptor_t*)handler)->options;
    if (!print_extra) {
        options_print_variables(options);
    } else {
        options_print_variables_extra(options);
    }
    return DLB_SUCCESS;
}

#pragma GCC visibility pop
