/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#include "apis/dlb.h"

#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/env.h"
#include "support/error.h"
#include "support/debug.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#pragma GCC visibility push(default)

/* Status */

int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args) {
    spd_enter_dlb(NULL);
    if (__sync_bool_compare_and_swap(&thread_spd->dlb_initialized, false, true) ) {
        return Initialize(thread_spd, getpid(), ncpus, mask, dlb_args);
    } else {
        return DLB_ERR_INIT;
    }
}

int DLB_Finalize(void) {
    spd_enter_dlb(NULL);
    thread_spd->dlb_initialized = false;
    thread_spd->dlb_preinitialized = false;
    return Finish(thread_spd);
}

int DLB_PreInit(const_dlb_cpu_set_t mask, char ***next_environ) {
    spd_enter_dlb(NULL);
    if (!thread_spd->dlb_initialized
            && __sync_bool_compare_and_swap(&thread_spd->dlb_preinitialized, false, true)) {

        /* DLB_PreInit must modify the environment so that a process child
         * knows what CPUs must inherit */
        char flag[32];
        snprintf(flag, 32, "--preinit-pid=%d", getpid());
        dlb_setenv("DLB_ARGS", flag, next_environ, ENV_APPEND);

        return PreInitialize(thread_spd, mask, NULL);
    } else {
        return DLB_ERR_INIT;
    }
}

int DLB_Enable(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_lewi_enabled(thread_spd, true);
}

int DLB_Disable(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_lewi_enabled(thread_spd, false);
}

int DLB_SetMaxParallelism(int max) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_max_parallelism(thread_spd, max);
}

int DLB_UnsetMaxParallelism(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return unset_max_parallelism(thread_spd);
}


/* Callbacks */

int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg) {
    spd_enter_dlb(NULL);
    return pm_callback_set(&thread_spd->pm, which, callback, arg);
}

int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg) {
    spd_enter_dlb(NULL);
    return pm_callback_get(&thread_spd->pm, which, callback, arg);
}


/* Lend */

int DLB_Lend(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend(thread_spd);
}

int DLB_LendCpu(int cpuid) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpu(thread_spd, cpuid);
}

int DLB_LendCpus(int ncpus) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpus(thread_spd, ncpus);
}

int DLB_LendCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpu_mask(thread_spd, mask);
}


/* Reclaim */

int DLB_Reclaim(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim(thread_spd);
}

int DLB_ReclaimCpu(int cpuid) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpu(thread_spd, cpuid);
}

int DLB_ReclaimCpus(int ncpus) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpus(thread_spd, ncpus);
}

int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpu_mask(thread_spd, mask);
}


/* Acquire */

int DLB_AcquireCpu(int cpuid) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpu(thread_spd, cpuid);
}

int DLB_AcquireCpus(int ncpus) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpus(thread_spd, ncpus);
}

int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpu_mask(thread_spd, mask);
}

int DLB_AcquireCpusInMask(int ncpus, const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpus_in_mask(thread_spd, ncpus, mask);
}


/* Borrow */

int DLB_Borrow(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow(thread_spd);
}

int DLB_BorrowCpu(int cpuid) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpu(thread_spd, cpuid);
}

int DLB_BorrowCpus(int ncpus) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpus(thread_spd, ncpus);
}

int DLB_BorrowCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpu_mask(thread_spd, mask);
}

int DLB_BorrowCpusInMask(int ncpus, const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpus_in_mask(thread_spd, ncpus, mask);
}


/* Return */

int DLB_Return(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_all(thread_spd);
}

int DLB_ReturnCpu(int cpuid) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_cpu(thread_spd, cpuid);
}

int DLB_ReturnCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_cpu_mask(thread_spd, mask);
}


/* DROM Responsive */

int DLB_PollDROM(int *ncpus, dlb_cpu_set_t mask) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return poll_drom(thread_spd, ncpus, mask);
}

int DLB_PollDROM_Update(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return poll_drom_update(thread_spd);
}


/* Misc */

int DLB_CheckCpuAvailability(int cpuid) {
    spd_enter_dlb(NULL);
    return check_cpu_availability(thread_spd, cpuid);
}

int DLB_Barrier(void) {
    spd_enter_dlb(NULL);
    return node_barrier(thread_spd, NULL);
}

int DLB_BarrierAttach(void) {
    spd_enter_dlb(NULL);
    return node_barrier_attach(thread_spd, NULL);
}

int DLB_BarrierDetach(void) {
    spd_enter_dlb(NULL);
    return node_barrier_detach(thread_spd, NULL);
}

dlb_barrier_t* DLB_BarrierNamedRegister(const char *barrier_name,
        dlb_barrier_flags_t flags) {
    spd_enter_dlb(NULL);
    return (dlb_barrier_t*)node_barrier_register(thread_spd, barrier_name, flags);
}

dlb_barrier_t* DLB_BarrierNamedGet(const char *barrier_name,
        dlb_barrier_flags_t flags)
    __attribute__((alias("DLB_BarrierNamedRegister")));

int DLB_BarrierNamed(dlb_barrier_t *barrier) {
    spd_enter_dlb(NULL);
    return node_barrier(thread_spd, (barrier_t*)barrier);
}

int DLB_BarrierNamedAttach(dlb_barrier_t *barrier) {
    spd_enter_dlb(NULL);
    return node_barrier_attach(thread_spd, (barrier_t*)barrier);
}

int DLB_BarrierNamedDetach(dlb_barrier_t *barrier) {
    spd_enter_dlb(NULL);
    return node_barrier_detach(thread_spd, (barrier_t*)barrier);
}

int DLB_SetVariable(const char *variable, const char *value) {
    spd_enter_dlb(NULL);
    return options_set_variable(&thread_spd->options, variable, value);
}

int DLB_GetVariable(const char *variable, char *value) {
    spd_enter_dlb(NULL);
    return options_get_variable(&thread_spd->options, variable, value);
}

int DLB_PrintVariables(int print_extended) {
    spd_enter_dlb(NULL);
    options_print_variables(&thread_spd->options, print_extended);
    return DLB_SUCCESS;
}

int DLB_PrintShmem(int num_columns, dlb_printshmem_flags_t print_flags) {
    spd_enter_dlb(NULL);
    return print_shmem(thread_spd, num_columns, print_flags);
}

const char* DLB_Strerror(int errnum) {
    return error_get_str(errnum);
}

int DLB_SetObserverRole(bool thread_is_observer) {
    spd_enter_dlb(NULL);
    return set_observer_role(thread_is_observer);
}

#pragma GCC visibility pop
