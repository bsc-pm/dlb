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

#include "LB_numThreads/numThreads.h"

#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <pthread.h>

#define OMP_SYMBOLS_DEFINED ( \
        omp_set_num_threads \
        )

/* Weak symbols */
void omp_set_num_threads(int nthreads) __attribute__((weak));
int omp_get_max_threads(void) __attribute__((weak));

static void packed_omp_set_num_threads(int nthreads, void *arg) {
    omp_set_num_threads(nthreads);
}


void pm_init(pm_interface_t *pm) {

    *pm = (const pm_interface_t) {};

    /* OpenMP */
    if (OMP_SYMBOLS_DEFINED) {
        pm->dlb_callback_set_num_threads_ptr = packed_omp_set_num_threads;
    }
    /* Undefined */
    else {}
}

void pm_finalize(pm_interface_t *pm) {
    /* Reset all fields */
    *pm = (const pm_interface_t) {};
}

int pm_get_num_threads(void) {
    return omp_get_max_threads ? omp_get_max_threads() : 0;
}

int pm_callback_set(pm_interface_t *pm, dlb_callbacks_t which,
        dlb_callback_t callback, void *arg) {
    switch(which) {
        case dlb_callback_set_num_threads:
            pm->dlb_callback_set_num_threads_ptr = (dlb_callback_set_num_threads_t)callback;
            pm->dlb_callback_set_num_threads_arg = arg;
            break;
        case dlb_callback_set_active_mask:
            pm->dlb_callback_set_active_mask_ptr = (dlb_callback_set_active_mask_t)callback;
            pm->dlb_callback_set_active_mask_arg = arg;
            break;
        case dlb_callback_set_process_mask:
            pm->dlb_callback_set_process_mask_ptr = (dlb_callback_set_process_mask_t)callback;
            pm->dlb_callback_set_process_mask_arg = arg;
            break;
        case dlb_callback_add_active_mask:
            pm->dlb_callback_add_active_mask_ptr = (dlb_callback_add_active_mask_t)callback;
            pm->dlb_callback_add_active_mask_arg = arg;
            break;
        case dlb_callback_add_process_mask:
            pm->dlb_callback_add_process_mask_ptr = (dlb_callback_add_process_mask_t)callback;
            pm->dlb_callback_add_process_mask_arg = arg;
            break;
        case dlb_callback_enable_cpu:
            pm->dlb_callback_enable_cpu_ptr = (dlb_callback_enable_cpu_t)callback;
            pm->dlb_callback_enable_cpu_arg = arg;
            break;
        case dlb_callback_disable_cpu:
            pm->dlb_callback_disable_cpu_ptr = (dlb_callback_disable_cpu_t)callback;
            pm->dlb_callback_disable_cpu_arg = arg;
            break;
        default:
            return DLB_ERR_NOCBK;
    }
    return DLB_SUCCESS;
}

int pm_callback_get(const pm_interface_t *pm, dlb_callbacks_t which,
        dlb_callback_t *callback, void **arg) {
    switch(which) {
        case dlb_callback_set_num_threads:
            *callback = (dlb_callback_t)pm->dlb_callback_set_num_threads_ptr;
            *arg = pm->dlb_callback_set_num_threads_arg;
            break;
        case dlb_callback_set_active_mask:
            *callback = (dlb_callback_t)pm->dlb_callback_set_active_mask_ptr;
            *arg = pm->dlb_callback_set_active_mask_arg;
            break;
        case dlb_callback_set_process_mask:
            *callback = (dlb_callback_t)pm->dlb_callback_set_process_mask_ptr;
            *arg = pm->dlb_callback_set_process_mask_arg;
            break;
        case dlb_callback_add_active_mask:
            *callback = (dlb_callback_t)pm->dlb_callback_add_active_mask_ptr;
            *arg = pm->dlb_callback_add_active_mask_arg;
            break;
        case dlb_callback_add_process_mask:
            *callback = (dlb_callback_t)pm->dlb_callback_add_process_mask_ptr;
            *arg = pm->dlb_callback_add_process_mask_arg;
            break;
        case dlb_callback_enable_cpu:
            *callback = (dlb_callback_t)pm->dlb_callback_enable_cpu_ptr;
            *arg = pm->dlb_callback_enable_cpu_arg;
            break;
        case dlb_callback_disable_cpu:
            *callback = (dlb_callback_t)pm->dlb_callback_disable_cpu_ptr;
            *arg = pm->dlb_callback_disable_cpu_arg;
            break;
        default:
            return DLB_ERR_NOCBK;
    }
    return DLB_SUCCESS;
}

int update_threads(const pm_interface_t *pm, int threads) {
    if (pm->dlb_callback_set_num_threads_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }

    int cpus_node = mu_get_system_size();
    if (threads > cpus_node) {
        warning("Trying to use more CPUS (%d) than available (%d)", threads, cpus_node);
        threads = cpus_node;
    } else if (threads < 1) {
        threads = 1;
    }

    add_event(THREADS_USED_EVENT, threads);

    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_set_num_threads_ptr(threads, pm->dlb_callback_set_num_threads_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int set_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_set_active_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_set_active_mask_ptr(cpu_set, pm->dlb_callback_set_active_mask_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int set_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_set_process_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_set_process_mask_ptr(cpu_set, pm->dlb_callback_set_process_mask_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int add_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_add_active_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_add_active_mask_ptr(cpu_set, pm->dlb_callback_add_active_mask_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int add_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_add_process_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_add_process_mask_ptr(cpu_set, pm->dlb_callback_add_process_mask_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int enable_cpu(const pm_interface_t *pm, int cpuid) {
    /* fallback case */
    if (pm->dlb_callback_enable_cpu_ptr == NULL
            && pm->dlb_callback_add_active_mask_ptr) {
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(cpuid, &cpu_set);
        return add_mask(pm, &cpu_set);
    }

    if (pm->dlb_callback_enable_cpu_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_enable_cpu_ptr(cpuid, pm->dlb_callback_enable_cpu_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}

int disable_cpu(const pm_interface_t *pm, int cpuid) {
    /* fallback case */
    if (pm->dlb_callback_disable_cpu_ptr == NULL
            && pm->dlb_callback_set_active_mask_ptr) {
        cpu_set_t cpu_set;
        sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);
        CPU_CLR(cpuid, &cpu_set);
        return set_mask(pm, &cpu_set);
    }

    if (pm->dlb_callback_disable_cpu_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    instrument_event(CALLBACK_EVENT, 1, EVENT_BEGIN);
    pm->dlb_callback_disable_cpu_ptr(cpuid, pm->dlb_callback_disable_cpu_arg);
    instrument_event(CALLBACK_EVENT, 0, EVENT_END);
    return DLB_SUCCESS;
}
