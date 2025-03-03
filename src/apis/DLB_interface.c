/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "apis/dlb.h"

#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/env.h"
#include "support/error.h"
#include "support/dlb_common.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/* Status */

DLB_EXPORT_SYMBOL
int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args) {
    spd_enter_dlb(thread_spd);
    if (__sync_bool_compare_and_swap(&thread_spd->dlb_initialized, false, true) ) {
        return Initialize(thread_spd, getpid(), ncpus, mask, dlb_args);
    } else {
        return DLB_ERR_INIT;
    }
}

DLB_EXPORT_SYMBOL
int DLB_Finalize(void) {
    spd_enter_dlb(thread_spd);
    if (__sync_bool_compare_and_swap(&thread_spd->dlb_initialized, true, false) ) {
        return Finish(thread_spd);
    } else if (__sync_bool_compare_and_swap(&thread_spd->dlb_preinitialized, true, false) ) {
        /* This is to support the case when a single process does DLB_PreInit + DLB_Init
         * The first DLB_Finalize must finalize everything except the CPUs not inherited
         * during DLB_Init, if any. The second finalize must clean up the procinfo shmem.
         */
        shmem_procinfo__finalize(thread_spd->id,
                thread_spd->options.debug_opts & DBG_RETURNSTOLEN,
                thread_spd->options.shm_key);
        return DLB_SUCCESS;
    }
    return DLB_NOUPDT;
}

DLB_EXPORT_SYMBOL
int DLB_PreInit(const_dlb_cpu_set_t mask, char ***next_environ) {
    spd_enter_dlb(thread_spd);
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

DLB_EXPORT_SYMBOL
int DLB_Enable(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_lewi_enabled(thread_spd, true);
}

DLB_EXPORT_SYMBOL
int DLB_Disable(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_lewi_enabled(thread_spd, false);
}

DLB_EXPORT_SYMBOL
int DLB_SetMaxParallelism(int max) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return set_max_parallelism(thread_spd, max);
}

DLB_EXPORT_SYMBOL
int DLB_UnsetMaxParallelism(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return unset_max_parallelism(thread_spd);
}


/* Callbacks */

DLB_EXPORT_SYMBOL
int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg) {
    spd_enter_dlb(thread_spd);
    return pm_callback_set(&thread_spd->pm, which, callback, arg);
}

DLB_EXPORT_SYMBOL
int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg) {
    spd_enter_dlb(thread_spd);
    return pm_callback_get(&thread_spd->pm, which, callback, arg);
}


/* Lend */

DLB_EXPORT_SYMBOL
int DLB_Lend(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend(thread_spd);
}

DLB_EXPORT_SYMBOL
int DLB_LendCpu(int cpuid) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpu(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_LendCpus(int ncpus) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpus(thread_spd, ncpus);
}

DLB_EXPORT_SYMBOL
int DLB_LendCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return lend_cpu_mask(thread_spd, mask);
}


/* Reclaim */

DLB_EXPORT_SYMBOL
int DLB_Reclaim(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim(thread_spd);
}

DLB_EXPORT_SYMBOL
int DLB_ReclaimCpu(int cpuid) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpu(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_ReclaimCpus(int ncpus) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpus(thread_spd, ncpus);
}

DLB_EXPORT_SYMBOL
int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return reclaim_cpu_mask(thread_spd, mask);
}


/* Acquire */

DLB_EXPORT_SYMBOL
int DLB_AcquireCpu(int cpuid) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpu(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_AcquireCpus(int ncpus) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpus(thread_spd, ncpus);
}

DLB_EXPORT_SYMBOL
int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpu_mask(thread_spd, mask);
}

DLB_EXPORT_SYMBOL
int DLB_AcquireCpusInMask(int ncpus, const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return acquire_cpus_in_mask(thread_spd, ncpus, mask);
}


/* Borrow */

DLB_EXPORT_SYMBOL
int DLB_Borrow(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow(thread_spd);
}

DLB_EXPORT_SYMBOL
int DLB_BorrowCpu(int cpuid) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpu(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_BorrowCpus(int ncpus) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpus(thread_spd, ncpus);
}

DLB_EXPORT_SYMBOL
int DLB_BorrowCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpu_mask(thread_spd, mask);
}

DLB_EXPORT_SYMBOL
int DLB_BorrowCpusInMask(int ncpus, const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return borrow_cpus_in_mask(thread_spd, ncpus, mask);
}


/* Return */

DLB_EXPORT_SYMBOL
int DLB_Return(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_all(thread_spd);
}

DLB_EXPORT_SYMBOL
int DLB_ReturnCpu(int cpuid) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_cpu(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_ReturnCpuMask(const_dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return return_cpu_mask(thread_spd, mask);
}


/* DROM Responsive */

DLB_EXPORT_SYMBOL
int DLB_PollDROM(int *ncpus, dlb_cpu_set_t mask) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return poll_drom(thread_spd, ncpus, mask);
}

DLB_EXPORT_SYMBOL
int DLB_PollDROM_Update(void) {
    spd_enter_dlb(thread_spd);
    if (unlikely(!thread_spd->dlb_initialized)) {
        return DLB_ERR_NOINIT;
    }
    return poll_drom_update(thread_spd);
}


/* Misc */

DLB_EXPORT_SYMBOL
int DLB_CheckCpuAvailability(int cpuid) {
    spd_enter_dlb(thread_spd);
    return check_cpu_availability(thread_spd, cpuid);
}

DLB_EXPORT_SYMBOL
int DLB_Barrier(void) {
    spd_enter_dlb(thread_spd);
    return node_barrier(thread_spd, NULL);
}

DLB_EXPORT_SYMBOL
int DLB_BarrierAttach(void) {
    spd_enter_dlb(thread_spd);
    return node_barrier_attach(thread_spd, NULL);
}

DLB_EXPORT_SYMBOL
int DLB_BarrierDetach(void) {
    spd_enter_dlb(thread_spd);
    return node_barrier_detach(thread_spd, NULL);
}

DLB_EXPORT_SYMBOL
dlb_barrier_t* DLB_BarrierNamedRegister(const char *barrier_name,
        dlb_barrier_flags_t flags) {
    spd_enter_dlb(thread_spd);
    return (dlb_barrier_t*)node_barrier_register(thread_spd, barrier_name, flags);
}

DLB_EXPORT_SYMBOL
dlb_barrier_t* DLB_BarrierNamedGet(const char *barrier_name,
        dlb_barrier_flags_t flags)
    __attribute__((alias("DLB_BarrierNamedRegister")));

DLB_EXPORT_SYMBOL
int DLB_BarrierNamed(dlb_barrier_t *barrier) {
    spd_enter_dlb(thread_spd);
    return node_barrier(thread_spd, (barrier_t*)barrier);
}

DLB_EXPORT_SYMBOL
int DLB_BarrierNamedAttach(dlb_barrier_t *barrier) {
    spd_enter_dlb(thread_spd);
    return node_barrier_attach(thread_spd, (barrier_t*)barrier);
}

DLB_EXPORT_SYMBOL
int DLB_BarrierNamedDetach(dlb_barrier_t *barrier) {
    spd_enter_dlb(thread_spd);
    return node_barrier_detach(thread_spd, (barrier_t*)barrier);
}

DLB_EXPORT_SYMBOL
int DLB_SetVariable(const char *variable, const char *value) {
    spd_enter_dlb(thread_spd);
    return options_set_variable(&thread_spd->options, variable, value);
}

DLB_EXPORT_SYMBOL
int DLB_GetVariable(const char *variable, char *value) {
    spd_enter_dlb(thread_spd);
    return options_get_variable(&thread_spd->options, variable, value);
}

DLB_EXPORT_SYMBOL
int DLB_PrintVariables(int print_extended) {
    spd_enter_dlb(thread_spd);
    options_print_variables(&thread_spd->options, print_extended);
    return DLB_SUCCESS;
}

DLB_EXPORT_SYMBOL
int DLB_PrintShmem(int num_columns, dlb_printshmem_flags_t print_flags) {
    spd_enter_dlb(thread_spd);
    return print_shmem(thread_spd, num_columns, print_flags);
}

DLB_EXPORT_SYMBOL
const char* DLB_Strerror(int errnum) {
    return error_get_str(errnum);
}

DLB_EXPORT_SYMBOL
int DLB_SetObserverRole(bool thread_is_observer) {
    spd_enter_dlb(thread_spd);
    return set_observer_role(thread_is_observer);
}

DLB_EXPORT_SYMBOL
int DLB_GetVersion(int *major, int *minor, int *patch) {
    if (major) *major = DLB_VERSION_MAJOR;
    if (minor) *minor = DLB_VERSION_MINOR;
    if (patch) *patch = DLB_VERSION_PATCH;
    return DLB_SUCCESS;
}
