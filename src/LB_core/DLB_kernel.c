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

#include "LB_core/DLB_kernel.h"

#include "LB_core/spd.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/DLB_interface.h"
#include "support/debug.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <string.h>


/* Status */

int Initialize(subprocess_descriptor_t *spd, pid_t id, int ncpus,
        const cpu_set_t *mask, const char *lb_args) {

    int error = DLB_SUCCESS;

    // Initialize first instrumentation module
    options_init(&spd->options, lb_args);
    init_tracing(&spd->options);
    add_event(RUNTIME_EVENT, EVENT_INIT);

    // Infer LeWI mode
    spd->lb_policy = !spd->options.lewi ? POLICY_NONE :
        spd->options.preinit_pid ? POLICY_LEWI_MASK :
        mask ? POLICY_LEWI_MASK :
        POLICY_LEWI;

    // Initialize the rest of the subprocess descriptor
    pm_init(&spd->pm);
    set_lb_funcs(&spd->lb_funcs, spd->lb_policy);
    spd->id = spd->options.preinit_pid ? spd->options.preinit_pid : id;
    if (mask) {
        // Preferred case, mask is provided by the user
        memcpy(&spd->process_mask, mask, sizeof(cpu_set_t));
    } else if (spd->lb_policy == POLICY_LEWI) {
        // If LeWI, we don't want the process mask, just a mask of size 'ncpus'
        if (ncpus <= 0) ncpus = pm_get_num_threads();
        CPU_ZERO(&spd->process_mask);
        int i;
        for (i=0; i<ncpus; ++i) CPU_SET(i, &spd->process_mask);
    } else if (spd->lb_policy == POLICY_LEWI_MASK || spd->options.drom) {
        // These modes require mask support, best effort querying the system
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
        memcpy(&spd->process_mask, &process_mask, sizeof(cpu_set_t));
    }

    // Initialize modules
    debug_init(&spd->options);
    mu_init();
    timer_init();
    if (spd->lb_policy == POLICY_LEWI_MASK
            || spd->options.drom
            || spd->options.statistics
            || spd->options.preinit_pid) {

        // Initialize procinfo
        cpu_set_t new_process_mask;
        error = shmem_procinfo__init(spd->id, &spd->process_mask,
                &new_process_mask, spd->options.shm_key);

        // If the process has been pre-initialized (error=DLB_NOTED),
        // the mask provided by shmem_procinfo__init must overwrite the process mask
        if (error == DLB_NOTED) {
            set_process_mask(&spd->pm, &new_process_mask);
            memcpy(&spd->process_mask, &new_process_mask, sizeof(cpu_set_t));
            error = DLB_SUCCESS;
        }

        if (error != DLB_SUCCESS) return error;

        // Initialize cpuinfo
        error = shmem_cpuinfo__init(spd->id, &spd->process_mask, spd->options.shm_key);
        if (error != DLB_SUCCESS) return error;
    }
    if (spd->options.barrier) {
        shmem_barrier_init(spd->options.shm_key);
    }
    if (spd->options.mode == MODE_ASYNC) {
        error = shmem_async_init(spd->id, &spd->pm, &spd->process_mask, spd->options.shm_key);
        if (error != DLB_SUCCESS) return error;
    }

    // Initialise LeWI
    error = spd->lb_funcs.init(spd);
    if (error != DLB_SUCCESS) return error;

    spd->dlb_enabled = true;
    add_event(DLB_MODE_EVENT, EVENT_ENABLED);
    add_event(RUNTIME_EVENT, EVENT_USER);

    // Print initialization summary
    info0("%s %s", PACKAGE, VERSION);
    if (spd->lb_policy != POLICY_NONE) {
        info0("Balancing policy: %s", policy_tostr(spd->lb_policy));
    }
    verbose(VB_API, "Enabled verbose mode for DLB API");
    verbose(VB_MPI_API, "Enabled verbose mode for MPI API");
    verbose(VB_MPI_INT, "Enabled verbose mode for MPI Interception");
    verbose(VB_SHMEM, "Enabled verbose mode for Shared Memory");
    verbose(VB_DROM, "Enabled verbose mode for DROM");
    verbose(VB_STATS, "Enabled verbose mode for STATS");
    verbose(VB_MICROLB, "Enabled verbose mode for microLB policies");
    verbose(VB_ASYNC, "Enabled verbose mode for Asynchronous thread");
    verbose(VB_OMPT, "Enabled verbose mode for OMPT experimental features");
    if (ncpus || mask) {
        info0("Number of CPUs: %d", ncpus ? ncpus : CPU_COUNT(&spd->process_mask));
    }

    return error;
}

int Finish(subprocess_descriptor_t *spd) {
    int error = DLB_SUCCESS;
    add_event(RUNTIME_EVENT, EVENT_FINALIZE);
    spd->dlb_enabled = false;

    if (spd->lb_funcs.finalize) {
        spd->lb_funcs.finalize(spd);
    }
    if (spd->options.mode == MODE_ASYNC) {
        shmem_async_finalize(spd->id);
    }
    if (spd->options.barrier) {
        shmem_barrier_finalize();
    }
    if (spd->lb_policy == POLICY_LEWI_MASK
            || spd->options.drom
            || spd->options.statistics
            || spd->options.preinit_pid) {
        shmem_cpuinfo__finalize(spd->id, spd->options.shm_key);
        shmem_procinfo__finalize(spd->id, spd->options.debug_opts & DBG_RETURNSTOLEN,
                spd->options.shm_key);
    }
    timer_finalize();
    add_event(RUNTIME_EVENT, EVENT_USER);
    return error;
}

int PreInitialize(subprocess_descriptor_t *spd, const cpu_set_t *mask) {

    // Initialize options
    options_init(&spd->options, NULL);

    // Initialize subprocess descriptor
    spd->lb_policy = POLICY_NONE;
    pm_init(&spd->pm);
    set_lb_funcs(&spd->lb_funcs, spd->lb_policy);
    spd->id = spd->options.preinit_pid;
    memcpy(&spd->process_mask, mask, sizeof(cpu_set_t));

    // Initialize modules
    int error = DLB_SUCCESS;
    error = error ? error : shmem_cpuinfo_ext__init(spd->options.shm_key);
    error = error ? error : shmem_procinfo_ext__init(spd->options.shm_key);
    error = error ? error : shmem_procinfo_ext__preinit(spd->id, mask, 0);
    error = error ? error : shmem_cpuinfo_ext__preinit(spd->id, mask, 0);
    error = error ? error : shmem_cpuinfo_ext__finalize();
    error = error ? error : shmem_procinfo_ext__finalize();
    return error;
}

int set_dlb_enabled(subprocess_descriptor_t *spd, bool enabled) {
    int error = DLB_SUCCESS;
    if (__sync_bool_compare_and_swap(&spd->dlb_enabled, !enabled, enabled)) {
        if (enabled) {
            spd->lb_funcs.enable(spd);
            add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        } else {
            spd->lb_funcs.disable(spd);
            add_event(DLB_MODE_EVENT, EVENT_DISABLED);
        }
    } else {
        error = DLB_NOUPDT;
    }
    return error;
}

int set_max_parallelism(subprocess_descriptor_t *spd, int max) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        error = spd->lb_funcs.set_max_parallelism(spd, max);
    }
    return error;
}


/* MPI specific */

void IntoCommunication(void) {
    const subprocess_descriptor_t *spd = get_global_spd();
    if (spd->dlb_enabled) {
        spd->lb_funcs.into_communication(spd);
    }
}

void OutOfCommunication(void) {
    const subprocess_descriptor_t *spd = get_global_spd();
    if (spd->dlb_enabled) {
        spd->lb_funcs.out_of_communication(spd);
    }
}

void IntoBlockingCall(int is_iter, int blocking_mode) {
    const subprocess_descriptor_t *spd = get_global_spd();
    if (spd->dlb_enabled) {
        spd->lb_funcs.into_blocking_call(spd);
    }
}

void OutOfBlockingCall(int is_iter) {
    const subprocess_descriptor_t *spd = get_global_spd();
    if (spd->dlb_enabled) {
        spd->lb_funcs.out_of_blocking_call(spd, is_iter);
    }
}


/* Lend */

int lend(const subprocess_descriptor_t *spd) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd->lb_funcs.lend(spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd->lb_funcs.lend_cpu(spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd->lb_funcs.lend_cpus(spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd->lb_funcs.lend_cpu_mask(spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Reclaim */

int reclaim(const subprocess_descriptor_t *spd) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd->lb_funcs.reclaim(spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd->lb_funcs.reclaim_cpu(spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd->lb_funcs.reclaim_cpus(spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd->lb_funcs.reclaim_cpu_mask(spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Acquire */

int acquire_cpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd->lb_funcs.acquire_cpu(spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd->lb_funcs.acquire_cpus(spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd->lb_funcs.acquire_cpu_mask(spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Borrow */

int borrow(const subprocess_descriptor_t *spd) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd->lb_funcs.borrow(spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd->lb_funcs.borrow_cpu(spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd->lb_funcs.borrow_cpus(spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd->lb_funcs.borrow_cpu_mask(spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Return */

int return_all(const subprocess_descriptor_t *spd) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd->lb_funcs.return_all(spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int return_cpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd->lb_funcs.return_cpu(spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int return_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error;
    if (!spd->dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd->lb_funcs.return_cpu_mask(spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Drom Responsive */

int poll_drom(const subprocess_descriptor_t *spd, int *new_cpus, cpu_set_t *new_mask) {
    int error;
    if (!spd->dlb_enabled || !spd->options.drom) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_POLLDROM);
        // Use a local mask if new_mask was not provided
        cpu_set_t local_mask;
        cpu_set_t *mask = new_mask ? new_mask : &local_mask;

        error = shmem_procinfo__polldrom(spd->id, new_cpus, mask);
        if (error == DLB_SUCCESS) {
            shmem_cpuinfo__update_ownership(spd->id, mask);
            spd->lb_funcs.update_ownership_info(spd, mask);
        }
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int poll_drom_update(const subprocess_descriptor_t *spd) {
    cpu_set_t new_mask;
    int error = poll_drom(spd, NULL, &new_mask);
    if (error == DLB_SUCCESS) {
        set_process_mask(&spd->pm, &new_mask);
    }
    return error;
}


/* Misc */

int check_cpu_availability(const subprocess_descriptor_t *spd, int cpuid) {
    int error = DLB_SUCCESS;
    if (spd->dlb_enabled) {
        error = spd->lb_funcs.check_cpu_availability(spd, cpuid);
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

int node_barrier(void) {
    add_event(RUNTIME_EVENT, EVENT_BARRIER);
    shmem_barrier();
    add_event(RUNTIME_EVENT, 0);
    return DLB_SUCCESS;
}

int print_shmem(subprocess_descriptor_t *spd, int num_columns,
        dlb_printshmem_flags_t print_flags) {
    if (!spd->dlb_initialized) {
        options_init(&spd->options, NULL);
        debug_init(&spd->options);
    }

    shmem_cpuinfo__print_info(spd->options.shm_key, num_columns, print_flags);
    shmem_procinfo__print_info(spd->options.shm_key);

    return DLB_SUCCESS;
}
