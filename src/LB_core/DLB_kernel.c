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
#include "support/debug.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>

/* These flags are used to
 *  a) activate/deactive DLB functionality from the API
 *  b) protect DLB functionality before the initialization
 *  c) protect DLB_Init / DLB_MPI_Init twice
 */
static bool dlb_enabled = false;
static bool dlb_initialized = false;

/* Temporary global sub-process descriptor */
static subprocess_descriptor_t spd;


/* Status */

int Initialize(int ncpus, const cpu_set_t *mask, const char *lb_args) {

    int error = DLB_SUCCESS;
    if (!dlb_initialized) {
        // Initialize first instrumentation module
        options_init(&spd.options, lb_args);
        init_tracing(&spd.options);
        add_event(RUNTIME_EVENT, EVENT_INIT);

        // Infer LeWI mode
        spd.lb_policy = !spd.options.lewi ? POLICY_NONE :
                                    !mask ? POLICY_LEWI :
                                            POLICY_LEWI_MASK;

        // Initialize the rest of the subprocess descriptor
        pm_init(&spd.pm);
        set_lb_funcs(&spd.lb_funcs, spd.lb_policy);
        spd.id = spd.options.preinit_pid ? spd.options.preinit_pid : getpid();
        spd.cpus_priority_array = malloc(mu_get_system_size()*sizeof(int));
        if (mask) {
            memcpy(&spd.process_mask, mask, sizeof(cpu_set_t));
        } else if (spd.lb_policy == POLICY_LEWI) {
            // We need to pass ncpus through spd, CPU order doesn't matter
            CPU_ZERO(&spd.process_mask);
            int i;
            for (i=0; i<ncpus; ++i) CPU_SET(i, &spd.process_mask);
        }

        // Initialize modules
        debug_init(&spd.options);
        if (spd.lb_policy == POLICY_LEWI_MASK || spd.options.drom || spd.options.statistics) {
            // Mandatory: obtain mask
            fatal_cond(!mask, "DROM and TALP modules require mask support");

            // If the process has been pre-initialized, the process mask may have changed
            // procinfo_init must return a new_mask if so
            cpu_set_t new_process_mask;
            CPU_ZERO(&new_process_mask);

            shmem_procinfo__init(spd.id, &spd.process_mask, &new_process_mask, spd.options.shm_key);
            if (CPU_COUNT(&new_process_mask) > 0) {
                memcpy(&spd.process_mask, &new_process_mask, sizeof(cpu_set_t));
                // FIXME: this will probably fail if we can't register a callback before Init
                set_process_mask(&spd.pm, &new_process_mask);
            }
            shmem_cpuinfo__init(spd.id, &spd.process_mask, spd.options.shm_key);
        }
        if (spd.options.barrier) {
            shmem_barrier_init(spd.options.shm_key);
        }
        if (spd.options.mode == MODE_ASYNC) {
            shmem_async_init(spd.id, &spd.pm, &spd.process_mask, spd.options.shm_key);
        }

        // Initialise LeWI
        spd.lb_funcs.init(&spd);
        spd.lb_funcs.update_priority_cpus(&spd, &spd.process_mask);

        dlb_enabled = true;
        dlb_initialized = true;
        add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        add_event(RUNTIME_EVENT, 0);

        // Print initialization summary
        info0("%s %s", PACKAGE, VERSION);
        if (spd.lb_policy != POLICY_NONE) {
            info0("Balancing policy: %s", policy_tostr(spd.lb_policy));
        }
        verbose(VB_API, "Enabled verbose mode for DLB API");
        verbose(VB_MPI_API, "Enabled verbose mode for MPI API");
        verbose(VB_MPI_INT, "Enabled verbose mode for MPI Interception");
        verbose(VB_SHMEM, "Enabled verbose mode for Shared Memory");
        verbose(VB_DROM, "Enabled verbose mode for DROM");
        verbose(VB_STATS, "Enabled verbose mode for STATS");
        verbose(VB_MICROLB, "Enabled verbose mode for microLB policies");
        if (ncpus || mask) {
            info0("Number of CPUs: %d", ncpus ? ncpus : CPU_COUNT(&spd.process_mask));
        }
    } else {
        error = DLB_ERR_INIT;
    }
    return error;
}

int Finish(void) {
    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        add_event(RUNTIME_EVENT, EVENT_FINALIZE);
        dlb_enabled = false;
        dlb_initialized = false;
        spd.lb_funcs.finalize(&spd);
        // Unload modules
        if (spd.options.mode == MODE_ASYNC) {
            shmem_async_finalize(spd.id);
        }
        if (spd.options.barrier) {
            shmem_barrier_finalize();
        }
        if (spd.lb_policy == POLICY_LEWI_MASK || spd.options.drom || spd.options.statistics) {
            shmem_cpuinfo__finalize(spd.id);
            shmem_procinfo__finalize(spd.id, spd.options.debug_opts & DBG_RETURNSTOLEN);
        }
        free(spd.cpus_priority_array);
        add_event(RUNTIME_EVENT, 0);
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}

int set_dlb_enabled(bool enabled) {
    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        if (__sync_bool_compare_and_swap(&dlb_enabled, !enabled, enabled)) {
            if (enabled) {
                spd.lb_funcs.enable(&spd);
                add_event(DLB_MODE_EVENT, EVENT_ENABLED);
            } else {
                spd.lb_funcs.disable(&spd);
                add_event(DLB_MODE_EVENT, EVENT_DISABLED);
            }
        } else {
            error = DLB_NOUPDT;
        }
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}

int set_max_parallelism(int max) {
    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        // do something with max
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}


/* Callbacks */

int callback_set(dlb_callbacks_t which, dlb_callback_t callback, void *arg) {
    return pm_callback_set(&spd.pm, which, callback, arg);
}

int callback_get(dlb_callbacks_t which, dlb_callback_t *callback, void **arg) {
    return pm_callback_get(&spd.pm, which, callback, arg);
}


/* MPI specific */

void IntoCommunication(void) {
    if (dlb_enabled) {
        spd.lb_funcs.into_communication(&spd);
    }
}

void OutOfCommunication(void) {
    if (dlb_enabled) {
        spd.lb_funcs.out_of_communication(&spd);
    }
}

void IntoBlockingCall(int is_iter, int blocking_mode) {
    if (dlb_enabled) {
        spd.lb_funcs.into_blocking_call(&spd);
    }
}

void OutOfBlockingCall(int is_iter) {
    if (dlb_enabled) {
        spd.lb_funcs.out_of_blocking_call(&spd, is_iter);
    }
}


/* Lend */

int lend(void) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd.lb_funcs.lend(&spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd.lb_funcs.lend_cpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpus(int ncpus) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd.lb_funcs.lend_cpus(&spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int lend_cpu_mask(const cpu_set_t *mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_LEND);
        error = spd.lb_funcs.lend_cpu_mask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Reclaim */

int reclaim(void) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd.lb_funcs.reclaim(&spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd.lb_funcs.reclaim_cpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpus(int ncpus) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd.lb_funcs.reclaim_cpus(&spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int reclaim_cpu_mask(const cpu_set_t *mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RECLAIM);
        error = spd.lb_funcs.reclaim_cpu_mask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Acquire */

int acquire_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquire_cpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpus(int ncpus) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquire_cpus(&spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpu_mask(const cpu_set_t *mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquire_cpu_mask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Borrow */

int borrow(void) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd.lb_funcs.borrow(&spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd.lb_funcs.borrow_cpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpus(int ncpus) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd.lb_funcs.borrow_cpus(&spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int borrow_cpu_mask(const cpu_set_t *mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_BORROW);
        error = spd.lb_funcs.borrow_cpu_mask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Return */

int return_all(void) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd.lb_funcs.return_all(&spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int return_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd.lb_funcs.return_cpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int return_cpu_mask(const cpu_set_t *mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        error = spd.lb_funcs.return_cpu_mask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Drom Responsive */

int poll_drom(int *new_cpus, cpu_set_t *new_mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled || !spd.options.drom) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_POLLDROM);
        // Use a local mask if new_mask was not provided
        cpu_set_t local_mask;
        cpu_set_t *mask = new_mask ? new_mask : &local_mask;

        error = shmem_procinfo__polldrom(spd.id, new_cpus, mask);
        if (error == DLB_SUCCESS) {
            shmem_cpuinfo__update_ownership(spd.id, mask);
            spd.lb_funcs.update_priority_cpus(&spd, mask);
        }
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int poll_drom_update(void) {
    cpu_set_t new_mask;
    int error = poll_drom(NULL, &new_mask);
    if (error == DLB_SUCCESS) {
        set_process_mask(&spd.pm, &new_mask);
    }
    return error;
}


/* Misc */

int check_cpu_availability(int cpuid) {
    int error = DLB_SUCCESS;
    if (dlb_enabled) {
        error = spd.lb_funcs.check_cpu_availability(&spd, cpuid);
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

int set_variable(const char *variable, const char *value) {
    return options_set_variable(&spd.options, variable, value);
}

int get_variable(const char *variable, char *value) {
    return options_get_variable(&spd.options, variable, value);
}

int print_variables(bool print_extra) {
    if (!print_extra) {
        options_print_variables(&spd.options);
    } else {
        options_print_variables_extra(&spd.options);
    }
    return DLB_SUCCESS;
}

int print_shmem(int num_columns) {
    if (!dlb_initialized) {
        options_init(&spd.options, NULL);
        debug_init(&spd.options);
    }

    /* color is true be default for now,
     * we could use some option in DLB_ARGS */
    bool color = true;

    shmem_cpuinfo_ext__init(spd.options.shm_key);
    shmem_cpuinfo__print_info(num_columns, color);
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__init(spd.options.shm_key);
    shmem_procinfo__print_info();
    shmem_procinfo_ext__finalize();
    return DLB_SUCCESS;
}

// Others
const options_t* get_global_options(void) {
    return &spd.options;
}
