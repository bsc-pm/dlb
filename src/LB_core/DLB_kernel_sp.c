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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_core/DLB_kernel_sp.h"

#include "LB_core/spd.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <string.h>
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

subprocess_descriptor_t* Initialize_sp(int ncpus, const cpu_set_t *mask, const char *lb_args) {
    subprocess_descriptor_t *spd = malloc(sizeof(subprocess_descriptor_t));

    // Initialize first instrumentation module
    options_init(&spd->options, lb_args);
    init_tracing(&spd->options);
    add_event(RUNTIME_EVENT, EVENT_INIT);

    // Initialize the rest of the subprocess descriptor
    pm_init(&spd->pm);
    policy_t policy = spd->options.lb_policy;
    set_lb_funcs(&spd->lb_funcs, policy);
    spd->id = get_atomic_spid();;
    spd->cpus_priority_array = malloc(mu_get_system_size()*sizeof(int));
    if (mask) {
        memcpy(&spd->process_mask, mask, sizeof(cpu_set_t));
    } else {
        sched_getaffinity(0, sizeof(cpu_set_t), &spd->process_mask);
    }


    // Initialize modules
    debug_init(&spd->options);
    if (policy != POLICY_NONE || spd->options.drom || spd->options.statistics) {
        // If the process has been pre-initialized, the process mask may have changed
        // procinfo_init must return a new_mask if so
        cpu_set_t new_process_mask;
        CPU_ZERO(&new_process_mask);

        shmem_procinfo__init(spd->id, &spd->process_mask, &new_process_mask, spd->options.shm_key);
        if (CPU_COUNT(&new_process_mask) > 0) {
            memcpy(&spd->process_mask, &new_process_mask, sizeof(cpu_set_t));
            // FIXME: this will probably fail if we can't register a callback before Init
            set_process_mask(&spd->pm, &new_process_mask);
        }
        shmem_cpuinfo__init(spd->id, &spd->process_mask, spd->options.shm_key);
    }
    if (spd->options.barrier) {
        shmem_barrier_init(spd->options.shm_key);
    }
    if (spd->options.mode == MODE_ASYNC) {
        shmem_async_init(spd->id, &spd->pm, spd->options.shm_key);
    }
    spd->lb_funcs.init(spd);
    spd->lb_funcs.update_priority_cpus(spd, &spd->process_mask);

    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    add_event(RUNTIME_EVENT, 0);

    // Print initialization summary
    info0("%s %s", PACKAGE, VERSION);
    info0("Balancing policy: %s", policy_tostr(spd->options.lb_policy));
    verbose(VB_API, "Enabled verbose mode for DLB API");
    verbose(VB_MPI_API, "Enabled verbose mode for MPI API");
    verbose(VB_MPI_INT, "Enabled verbose mode for MPI Interception");
    verbose(VB_SHMEM, "Enabled verbose mode for Shared Memory");
    verbose(VB_DROM, "Enabled verbose mode for DROM");
    verbose(VB_STATS, "Enabled verbose mode for STATS");
    verbose(VB_MICROLB, "Enabled verbose mode for microLB policies");
#ifdef MPI_LIB
    info0 ("MPI processes per node: %d", _mpis_per_node);
#endif
    info0("Number of CPUs: %d", CPU_COUNT(&spd->process_mask));

    return spd;
}

int Finish_sp(subprocess_descriptor_t *spd) {
    int error = DLB_SUCCESS;
    if (spd) {
        spd->lb_funcs.finalize(spd);
        // Unload modules
        if (spd->options.mode == MODE_ASYNC) {
            shmem_async_finalize(spd->id);
        }
        if (spd->options.barrier) {
            shmem_barrier_finalize();
        }
        policy_t policy = spd->options.lb_policy;
        if (policy != POLICY_NONE || spd->options.drom || spd->options.statistics) {
            shmem_cpuinfo__finalize(spd->id);
            shmem_procinfo__finalize(spd->id);
        }
        free(spd->cpus_priority_array);
        free(spd);
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}

int set_dlb_enabled_sp(subprocess_descriptor_t *spd, bool enabled) {
    return DLB_ERR_NOCOMP;
}

int set_max_parallelism_sp(subprocess_descriptor_t *spd, int max) {
    int error = DLB_SUCCESS;
    // do something with max
    return error;
}


/* Callbacks */

int callback_set_sp(subprocess_descriptor_t *spd, dlb_callbacks_t which, dlb_callback_t callback) {
    return pm_callback_set(&spd->pm, which, callback);
}

int callback_get_sp(subprocess_descriptor_t *spd, dlb_callbacks_t which, dlb_callback_t *callback) {
    return pm_callback_get(&spd->pm, which, callback);
}


/* Lend */

int lend_sp(subprocess_descriptor_t *spd) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    int error = spd->lb_funcs.lend(spd);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int lend_cpu_sp(subprocess_descriptor_t *spd, int cpuid) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    int error = spd->lb_funcs.lend_cpu(spd, cpuid);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int lend_cpus_sp(subprocess_descriptor_t *spd, int ncpus) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    int error = spd->lb_funcs.lend_cpus(spd, ncpus);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int lend_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    add_event(RUNTIME_EVENT, EVENT_LEND);
    int error = spd->lb_funcs.lend_cpu_mask(spd, mask);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}


/* Reclaim */

int reclaim_sp(subprocess_descriptor_t *spd) {
    add_event(RUNTIME_EVENT, EVENT_RECLAIM);
    int error = spd->lb_funcs.reclaim(spd);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int reclaim_cpu_sp(subprocess_descriptor_t *spd, int cpuid) {
    add_event(RUNTIME_EVENT, EVENT_RECLAIM);
    int error = spd->lb_funcs.reclaim_cpu(spd, cpuid);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int reclaim_cpus_sp(subprocess_descriptor_t *spd, int ncpus) {
    add_event(RUNTIME_EVENT, EVENT_RECLAIM);
    int error = spd->lb_funcs.reclaim_cpus(spd, ncpus);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int reclaim_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    add_event(RUNTIME_EVENT, EVENT_RECLAIM);
    int error = spd->lb_funcs.reclaim_cpu_mask(spd, mask);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}


/* Acquire */

int acquire_cpu_sp(subprocess_descriptor_t *spd, int cpuid) {
    add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
    int error = spd->lb_funcs.acquire_cpu(spd, cpuid);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int acquire_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
    int error = spd->lb_funcs.acquire_cpu_mask(spd, mask);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}


/* Borrow */

int borrow_sp(subprocess_descriptor_t *spd) {
    add_event(RUNTIME_EVENT, EVENT_BORROW);
    int error = spd->lb_funcs.borrow(spd);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int borrow_cpu_sp(subprocess_descriptor_t *spd, int cpuid) {
    add_event(RUNTIME_EVENT, EVENT_BORROW);
    int error = spd->lb_funcs.borrow_cpu(spd, cpuid);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int borrow_cpus_sp(subprocess_descriptor_t *spd, int ncpus) {
    add_event(RUNTIME_EVENT, EVENT_BORROW);
    int error = spd->lb_funcs.borrow_cpus(spd, ncpus);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int borrow_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    add_event(RUNTIME_EVENT, EVENT_BORROW);
    int error = spd->lb_funcs.borrow_cpu_mask(spd, mask);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}


/* Return */

int return_all_sp(subprocess_descriptor_t *spd) {
    add_event(RUNTIME_EVENT, EVENT_RETURN);
    int error = spd->lb_funcs.return_all(spd);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int return_cpu_sp(subprocess_descriptor_t *spd, int cpuid) {
    add_event(RUNTIME_EVENT, EVENT_RETURN);
    int error = spd->lb_funcs.return_cpu(spd, cpuid);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int return_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    add_event(RUNTIME_EVENT, EVENT_RETURN);
    int error = spd->lb_funcs.return_cpu_mask(spd, mask);
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}


/* Drom Responsive */

int poll_drom_sp(subprocess_descriptor_t *spd, int *new_cpus, cpu_set_t *new_mask) {
    int error;
    if (!spd->options.drom) {
        error = DLB_ERR_DISBLD;
    } else {
        // Use a local mask if new_mask was not provided
        cpu_set_t local_mask;
        cpu_set_t *mask = new_mask ? new_mask : &local_mask;

        error = shmem_procinfo__polldrom(spd->id, new_cpus, mask);
        if (error == DLB_SUCCESS) {
            shmem_cpuinfo__update_ownership(spd->id, mask);
            spd->lb_funcs.update_priority_cpus(spd, mask);
        }
    }
    return error;
}

int poll_drom_update_sp(subprocess_descriptor_t *spd) {
    cpu_set_t new_mask;
    int error = poll_drom_sp(spd, NULL, &new_mask);
    if (error == DLB_SUCCESS) {
        set_process_mask(&spd->pm, &new_mask);
    }
    return error;
}


/* Misc */

int set_variable_sp(subprocess_descriptor_t *spd, const char *variable, const char *value) {
    return options_set_variable(&spd->options, variable, value);
}

int get_variable_sp(subprocess_descriptor_t *spd, const char *variable, char *value) {
    return options_get_variable(&spd->options, variable, value);
}

int print_variables_sp(subprocess_descriptor_t *spd) {
    options_print_variables(&spd->options);
    return DLB_SUCCESS;
}
