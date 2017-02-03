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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <sched.h>
#include <string.h>
#include <unistd.h>

#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_barrier.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"


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

int Initialize(const cpu_set_t *mask, const char *lb_args) {

    int error = DLB_SUCCESS;
    if (!dlb_initialized) {
        // Initialize first instrumentation module
        options_init(&spd.options, lb_args);
        init_tracing(&spd.options);
        add_event(RUNTIME_EVENT, EVENT_INIT);

        // Initialize the rest of the subprocess descriptor
        pm_init(&spd.pm);
        policy_t policy = spd.options.lb_policy;
        set_lb_funcs(&spd.lb_funcs, policy);
        spd.process_id = getpid();
        if (mask) {
            memcpy(&spd.process_mask, mask, sizeof(cpu_set_t));
        } else {
            sched_getaffinity(0, sizeof(cpu_set_t), &spd.process_mask);
        }


        // Initialize modules
        debug_init(&spd.options);
        spd.lb_funcs.init(&spd);
        if (policy != POLICY_NONE || spd.options.drom || spd.options.statistics) {
            shmem_procinfo__init(&spd.process_mask, spd.options.shm_key);
            shmem_cpuinfo__init(&spd.process_mask, &spd.options.dlb_mask,
                    spd.options.shm_key);
        }
        if (spd.options.barrier) {
            shmem_barrier_init(spd.options.shm_key);
        }

        dlb_enabled = true;
        dlb_initialized = true;
        add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        add_event(RUNTIME_EVENT, 0);

        // Print initialization summary
        info0("%s %s", PACKAGE, VERSION);
        info0("Balancing policy: %s", policy_tostr(spd.options.lb_policy));
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
        info0("Number of threads: %d", CPU_COUNT(&spd.process_mask));
    } else {
        error = DLB_ERR_INIT;
    }
    return error;
}

int Finish(void) {
    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        dlb_enabled = false;
        dlb_initialized = false;
        spd.lb_funcs.finish(&spd);
        // Unload modules
        if (spd.options.barrier) {
            shmem_barrier_finalize();
        }
        policy_t policy = spd.options.lb_policy;
        if (policy != POLICY_NONE || spd.options.drom || spd.options.statistics) {
            shmem_cpuinfo__finalize();
            shmem_procinfo__finalize();
        }
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}

int set_dlb_enabled(bool enabled) {
    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        dlb_enabled = enabled;
        if (enabled){
            spd.lb_funcs.enableDLB(&spd);
            add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        }else{
            spd.lb_funcs.disableDLB(&spd);
            add_event(DLB_MODE_EVENT, EVENT_DISABLED);
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

int callback_set(dlb_callbacks_t which, dlb_callback_t callback) {
    return pm_callback_set(&spd.pm, which, callback);
}

int callback_get(dlb_callbacks_t which, dlb_callback_t callback) {
    return pm_callback_get(&spd.pm, which, callback);
}


/* MPI specific */

void IntoCommunication(void) {
    if (dlb_enabled) {
        spd.lb_funcs.intoCommunication(&spd);
    }
}

void OutOfCommunication(void) {
    if (dlb_enabled) {
        spd.lb_funcs.outOfCommunication(&spd);
    }
}

void IntoBlockingCall(int is_iter, int blocking_mode) {
    if (dlb_enabled) {
        spd.lb_funcs.intoBlockingCall(&spd);
    }
}

void OutOfBlockingCall(int is_iter) {
    if (dlb_enabled) {
        spd.lb_funcs.outOfBlockingCall(&spd, is_iter);
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
        error = spd.lb_funcs.lendCpu(&spd, cpuid);
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
        error = spd.lb_funcs.lendCpus(&spd, ncpus);
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
        error = spd.lb_funcs.lendCpuMask(&spd, mask);
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
        error = spd.lb_funcs.reclaimCpu(&spd, cpuid);
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
        error = spd.lb_funcs.reclaimCpus(&spd, ncpus);
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
        error = spd.lb_funcs.reclaimCpuMask(&spd, mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Acquire */

int acquire(void) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquire(&spd);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpu(int cpuid) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquireCpu(&spd, cpuid);
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
        error = spd.lb_funcs.acquireCpus(&spd, ncpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}

int acquire_cpu_mask(const cpu_set_t* mask) {
    int error;
    if (!dlb_initialized) {
        error = DLB_ERR_NOINIT;
    } else if (!dlb_enabled) {
        error = DLB_ERR_DISBLD;
    } else {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE);
        error = spd.lb_funcs.acquireCpuMask(&spd, mask);
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
        error = spd.lb_funcs.returnAll(&spd);
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
        error = spd.lb_funcs.returnCpu(&spd, cpuid);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
    return error;
}


/* Drom Responsive */

int poll_drom(int *new_threads, cpu_set_t *new_mask) {
    int error = DLB_ERR_UNKNOWN;
    if (dlb_enabled) {
        if (new_mask) {
            // If new_mask is provided by the user
            error = shmem_procinfo__update_new(
                    spd.options.drom, spd.options.statistics, new_threads, new_mask);
            if (error == DLB_SUCCESS) {
                shmem_cpuinfo__update_ownership(new_mask);
            }
        } else {
            // Otherwise, mask is allocated and freed
            cpu_set_t *mask = malloc(sizeof(cpu_set_t));
            error = shmem_procinfo__update_new(
                    spd.options.drom, spd.options.statistics, new_threads, mask);
            if (error == DLB_SUCCESS) {
                shmem_cpuinfo__update_ownership(mask);
            }
            free(mask);
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}


/* Misc */

int checkCpuAvailability(int cpuid) {
    int error = DLB_SUCCESS;
    if (dlb_enabled) {
        error = spd.lb_funcs.checkCpuAvailability(cpuid);
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

int barrier(void) {
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

int print_variables(void) {
    options_print_variables(&spd.options);
    return DLB_SUCCESS;
}

int print_shmem(void) {
    pm_init(&spd.pm);
    options_init(&spd.options, NULL);
    debug_init(&spd.options);
    shmem_cpuinfo_ext__init(spd.options.shm_key);
    shmem_cpuinfo_ext__print_info(spd.options.statistics);
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__init(spd.options.shm_key);
    shmem_procinfo_ext__print_info(spd.options.statistics);
    shmem_procinfo_ext__finalize();
    return DLB_SUCCESS;
}

// Others
const options_t* get_global_options(void) {
    return &spd.options;
}
