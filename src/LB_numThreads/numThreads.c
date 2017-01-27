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
#include <pthread.h>
#include "LB_numThreads/numThreads.h"
#include "LB_core/callbacks.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/mask_utils.h"

#define NANOS_SYMBOLS_DEFINED ( \
        nanos_omp_get_process_mask && \
        nanos_omp_set_process_mask && \
        nanos_omp_add_process_mask && \
        nanos_omp_get_active_mask && \
        nanos_omp_set_process_mask && \
        nanos_omp_add_process_mask && \
        nanos_omp_get_max_threads && \
        nanos_omp_set_num_threads \
        )

#define OMP_SYMBOLS_DEFINED ( \
        omp_get_max_threads && \
        omp_set_num_threads \
        )

/* Weak symbols */
void nanos_omp_get_process_mask(cpu_set_t *cpu_set) __attribute__((weak));
int  nanos_omp_set_process_mask(const cpu_set_t *cpu_set) __attribute__((weak));
void nanos_omp_add_process_mask(const cpu_set_t *cpu_set) __attribute__((weak));
void nanos_omp_get_active_mask(cpu_set_t *cpu_set) __attribute__((weak));
int  nanos_omp_set_active_mask(const cpu_set_t *cpu_set) __attribute__((weak));
void nanos_omp_add_active_mask(const cpu_set_t *cpu_set) __attribute__((weak));
int  nanos_omp_get_max_threads(void) __attribute__((weak));
void nanos_omp_set_num_threads(int nthreads) __attribute__((weak));
int  omp_get_max_threads(void) __attribute__((weak));
void omp_set_num_threads(int nthreads) __attribute__((weak));

static int cpus_node;

void pm_init(pm_interface_t *pm) {

    *pm = (pm_interface_t) {};
    cpus_node = mu_get_system_size();

    /* Nanos++ */
    if (NANOS_SYMBOLS_DEFINED) {
        pm->dlb_callback_set_num_threads_ptr = nanos_omp_set_num_threads;
        pm->dlb_callback_set_active_mask_ptr =
            (dlb_callback_set_active_mask_t)nanos_omp_set_active_mask;
        pm->dlb_callback_set_process_mask_ptr =
            (dlb_callback_set_process_mask_t)nanos_omp_set_process_mask;
        pm->dlb_callback_add_active_mask_ptr =
            (dlb_callback_add_active_mask_t)nanos_omp_add_active_mask;
        pm->dlb_callback_add_process_mask_ptr =
            (dlb_callback_add_process_mask_t)nanos_omp_add_process_mask;

        pm->dlb_getter_get_num_threads_ptr = nanos_omp_get_max_threads;
        pm->dlb_getter_get_active_mask_ptr =
            (dlb_getter_get_active_mask_t)nanos_omp_get_active_mask;
        pm->dlb_getter_get_process_mask_ptr =
            (dlb_getter_get_process_mask_t)nanos_omp_get_process_mask;
    }
    /* OpenMP */
    else if (OMP_SYMBOLS_DEFINED) {
        pm->dlb_callback_set_num_threads_ptr = omp_set_num_threads;

        pm->dlb_getter_get_num_threads_ptr = omp_get_max_threads;
    }
    /* Undefined */
    else {}

    // TODO remove global
    //_default_nthreads = (dlb_getter_get_num_threads_ptr != NULL) ?
    //dlb_getter_get_num_threads_ptr() : 1;
}

int pm_callback_set(pm_interface_t *pm, dlb_callbacks_t which, dlb_callback_t callback) {
    switch(which) {
        case dlb_callback_set_num_threads:
            pm->dlb_callback_set_num_threads_ptr = (dlb_callback_set_num_threads_t)callback;
            break;
        case dlb_callback_set_active_mask:
            pm->dlb_callback_set_active_mask_ptr = (dlb_callback_set_active_mask_t)callback;
            break;
        case dlb_callback_set_process_mask:
            pm->dlb_callback_set_process_mask_ptr = (dlb_callback_set_process_mask_t)callback;
            break;
        case dlb_callback_add_active_mask:
            pm->dlb_callback_add_active_mask_ptr = (dlb_callback_add_active_mask_t)callback;
            break;
        case dlb_callback_add_process_mask:
            pm->dlb_callback_add_process_mask_ptr = (dlb_callback_add_process_mask_t)callback;
            break;
        case dlb_callback_enable_cpu:
            pm->dlb_callback_enable_cpu_ptr = (dlb_callback_enable_cpu_t)callback;
            break;
        case dlb_callback_disable_cpu:
            pm->dlb_callback_disable_cpu_ptr = (dlb_callback_disable_cpu_t)callback;
            break;
        default:
            return DLB_ERR_NOCBK;
    }
    return DLB_SUCCESS;
}

int pm_callback_get(const pm_interface_t *pm, dlb_callbacks_t which, dlb_callback_t callback) {
    switch(which) {
        case dlb_callback_set_num_threads:
            callback = (dlb_callback_t)pm->dlb_callback_set_num_threads_ptr;
            break;
        case dlb_callback_set_active_mask:
            callback = (dlb_callback_t)pm->dlb_callback_set_active_mask_ptr;
            break;
        case dlb_callback_set_process_mask:
            callback = (dlb_callback_t)pm->dlb_callback_set_process_mask_ptr;
            break;
        case dlb_callback_add_active_mask:
            callback = (dlb_callback_t)pm->dlb_callback_set_active_mask_ptr;
            break;
        case dlb_callback_add_process_mask:
            callback = (dlb_callback_t)pm->dlb_callback_set_process_mask_ptr;
            break;
        default:
            return DLB_ERR_NOCBK;
    }
    return DLB_SUCCESS;
}

int pm_getter_set(pm_interface_t *pm, dlb_getters_t which, dlb_getter_t getter) {
    switch(which) {
        case dlb_getter_get_num_threads:
            pm->dlb_getter_get_num_threads_ptr = (dlb_getter_get_num_threads_t)getter;
            break;
        case dlb_getter_get_active_mask:
            pm->dlb_getter_get_active_mask_ptr = (dlb_getter_get_active_mask_t)getter;
            break;
        case dlb_getter_get_process_mask:
            pm->dlb_getter_get_process_mask_ptr = (dlb_getter_get_process_mask_t)getter;
            break;
        default:
            return DLB_ERR_NOGET;
    }
    return DLB_SUCCESS;
}

int pm_getter_get(const pm_interface_t *pm, dlb_getters_t which, dlb_getter_t getter) {
    switch(which) {
        case dlb_getter_get_num_threads:
            getter = (dlb_getter_t)pm->dlb_getter_get_num_threads_ptr;
            break;
        case dlb_getter_get_active_mask:
            getter = (dlb_getter_t)pm->dlb_getter_get_active_mask_ptr;
            break;
        case dlb_getter_get_process_mask:
            getter = (dlb_getter_t)pm->dlb_getter_get_process_mask_ptr;
            break;
        default:
            return DLB_ERR_NOGET;
    }
    return DLB_SUCCESS;
}


int update_threads(const pm_interface_t *pm, int threads) {
    if (pm->dlb_callback_set_num_threads_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }

    if (threads > cpus_node) {
        warning("Trying to use more CPUS (%d) than available (%d)", threads, cpus_node);
        threads = cpus_node;
    } else if (threads < 1) {
        warning("setting number of threads to 0 not allowed, falling back to 1");
        threads = 1;
    }

    add_event(THREADS_USED_EVENT, threads);

    pm->dlb_callback_set_num_threads_ptr(threads);
    return DLB_SUCCESS;
}

int set_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_set_active_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    pm->dlb_callback_set_active_mask_ptr(cpu_set);
    return DLB_SUCCESS;
}

int set_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_set_process_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    pm->dlb_callback_set_process_mask_ptr(cpu_set);
    return DLB_SUCCESS;
}

int add_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_add_active_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    pm->dlb_callback_add_active_mask_ptr(cpu_set);
    return DLB_SUCCESS;
}

int add_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set) {
    if (pm->dlb_callback_add_process_mask_ptr == NULL) {
        return DLB_ERR_NOCBK;
    }
    pm->dlb_callback_add_process_mask_ptr(cpu_set);
    return DLB_SUCCESS;
}

int enable_cpu(const pm_interface_t *pm, int cpuid) {
    if (pm->dlb_callback_enable_cpu_ptr == NULL) {
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(cpuid, &cpu_set);
        return add_mask(pm, &cpu_set);
    }
    pm->dlb_callback_enable_cpu_ptr(cpuid);
    return DLB_SUCCESS;
}

int disable_cpu(const pm_interface_t *pm, int cpuid) {
    if (pm->dlb_callback_disable_cpu_ptr == NULL) {
        cpu_set_t cpu_set;
        get_mask(pm, &cpu_set);
        CPU_CLR(cpuid, &cpu_set);
        return set_mask(pm, &cpu_set);
    }
    pm->dlb_callback_disable_cpu_ptr(cpuid);
    return DLB_SUCCESS;
}

int get_num_threads(const pm_interface_t *pm) {
    if (pm->dlb_getter_get_num_threads_ptr == NULL) {
        return 1;
    }
    return pm->dlb_getter_get_num_threads_ptr();
}

int get_mask(const pm_interface_t *pm, cpu_set_t *cpu_set) {
    if (pm->dlb_getter_get_active_mask_ptr == NULL) {
        sched_getaffinity(0, sizeof(cpu_set_t), cpu_set);
    } else {
        pm->dlb_getter_get_active_mask_ptr(cpu_set);
    }
    return DLB_SUCCESS;
}

int get_process_mask(const pm_interface_t *pm, cpu_set_t *cpu_set) {
    if (pm->dlb_getter_get_process_mask_ptr == NULL) {
        sched_getaffinity(0, sizeof(cpu_set_t), cpu_set);
    } else {
        pm->dlb_getter_get_process_mask_ptr(cpu_set);
    }
    return DLB_SUCCESS;
}
