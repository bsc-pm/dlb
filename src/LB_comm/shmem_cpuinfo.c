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

#include "LB_comm/shmem_cpuinfo.h"

#include "LB_comm/shmem.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "support/queues.h"
#include "support/small_array.h"
#include "support/atomic.h"

#include <limits.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

/* queue_pid_t */
#define QUEUE_T pid_t
#define QUEUE_SIZE 8
#include "support/queue_template.h"

/* queue_lewi_mask_request_t */
typedef struct {
    pid_t        pid;
    unsigned int howmany;
    cpu_set_t    allowed;
} lewi_mask_request_t;
#define QUEUE_T lewi_mask_request_t
#define QUEUE_KEY_T pid_t
#define QUEUE_SIZE 1024
#include "support/queue_template.h"


/* NOTE on default values:
 * The shared memory will be initialized to 0 when created,
 * thus all default values should represent 0
 * A CPU, by default, is DISABLED and owned by NOBODY
 */

enum { NOBODY = 0 };

typedef enum {
    CPU_DISABLED = 0,       // Not owned by any process nor part of the DLB mask
    CPU_BUSY,
    CPU_LENT
} cpu_state_t;

typedef struct {
    cpuid_t         id;                     // logical ID, or hwthread ID
    cpuid_t         core_id;                // core ID
    pid_t           owner;                  // Current owner
    pid_t           guest;                  // Current user of the CPU
    cpu_state_t     state;                  // owner's POV state (busy or lent)
    queue_pid_t     requests;               // List of PIDs requesting the CPU
} cpuinfo_t;

typedef struct cpuinfo_flags {
    bool initialized:1;
    bool queues_enabled:1;
    bool hw_has_smt:1;
} cpuinfo_flags_t;

typedef struct {
    cpuinfo_flags_t             flags;
    struct timespec             initial_time;
    atomic_int_least64_t        timestamp_cpu_lent;
    queue_lewi_mask_request_t   lewi_mask_requests;
    cpu_set_t                   free_cpus;          /* redundant info for speeding up queries:
                                                       lent, non-guested CPUs (idle) */
    cpu_set_t                   occupied_cores;     /* redundant info for speeding up queries:
                                                       lent or busy cores and guested by other
                                                       than the owner (lent or reclaimed) */
    cpuinfo_t                   node_info[];
} shdata_t;

enum { SHMEM_CPUINFO_VERSION = 6 };

static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int node_size;
static bool cpu_is_public_post_mortem = false;
static bool respect_cpuset = true;
static const char *shmem_name = "cpuinfo";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static inline bool is_idle(int cpu) __attribute__((unused));
static inline bool is_borrowed(pid_t pid, int cpu) __attribute__((unused));
static inline bool is_shmem_empty(void);


static void update_shmem_timestamp(void) {
    DLB_ATOMIC_ST_REL(&shdata->timestamp_cpu_lent, get_time_in_ns());
}

/* A core is eligible if all the CPUs in the core are not guested, or guested
 * by the process, and none of them are reclaimed */
static bool core_is_eligible(pid_t pid, int cpuid) {

    ensure(cpuid < node_size, "cpuid %d greater than node_size %d in %s",
            cpuid, node_size, __func__);

    if (shdata->flags.hw_has_smt) {
        const mu_cpuset_t *core_mask = mu_get_core_mask(cpuid);
        for (int cpuid_in_core = core_mask->first_cpuid;
                cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
                cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid_in_core];
            if ((cpuinfo->guest != pid && cpuinfo->guest != NOBODY)
                    || (cpuinfo->state == CPU_BUSY && cpuinfo->owner != pid)) {
                return false;
            }
        }
        return true;
    } else {
        const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        return cpuinfo->owner == pid
            || cpuinfo->guest == pid
            || cpuinfo->guest == NOBODY;
    }
}

/* A core is occupied if any of the CPUs in the core are guested by some
 * process that is not the owner. The owner is provided as parameter since
 * this function may be called during the core registration */
static bool core_is_occupied(pid_t owner, int cpuid) {

    ensure(cpuid < node_size, "cpuid %d greater than node_size %d in %s",
            cpuid, node_size, __func__);

    if (owner == NOBODY) return false;

    if (shdata->flags.hw_has_smt) {
        const mu_cpuset_t *core_mask = mu_get_core_mask(cpuid);
        for (int cpuid_in_core = core_mask->first_cpuid;
                cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
                cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
            pid_t guest = shdata->node_info[cpuid_in_core].guest;
            if (guest != NOBODY
                    && guest != owner) {
                return true;
            }
        }
        return false;
    } else {
        pid_t guest = shdata->node_info[cpuid].guest;
        return guest != NOBODY
            && guest != owner;
    }
}

/* A CPU is occupied if it's guested by some process that is not the owner. */
static bool cpu_is_occupied(pid_t owner, int cpuid) {

    ensure(cpuid < node_size, "cpuid %d greater than node_size %d in %s",
            cpuid, node_size, __func__);

    pid_t guest = shdata->node_info[cpuid].guest;
    return guest != NOBODY
        && owner != NOBODY
        && guest != owner;
}

/* Assuming that only cpuid has changed its state, update occupied_cores accordingly */
static void update_occupied_cores(pid_t owner, int cpuid) {

    ensure(cpuid < node_size, "cpuid %d greater than node_size %d in %s",
            cpuid, node_size, __func__);

    if (shdata->flags.hw_has_smt) {
        if (cpu_is_occupied(owner, cpuid)) {
            if (!CPU_ISSET(cpuid, &shdata->occupied_cores)) {
                // Core state has changed
                const cpu_set_t *core_mask = mu_get_core_mask(cpuid)->set;
                mu_or(&shdata->occupied_cores, &shdata->occupied_cores, core_mask);
            } else {
                // no change
            }
        } else {
            if (!CPU_ISSET(cpuid, &shdata->occupied_cores)) {
                // no change
            } else {
                // need to check all cores
                const cpu_set_t *core_mask = mu_get_core_mask(cpuid)->set;
                if (core_is_occupied(owner, cpuid)) {
                    mu_or(&shdata->occupied_cores, &shdata->occupied_cores, core_mask);
                } else {
                    mu_subtract(&shdata->occupied_cores, &shdata->occupied_cores, core_mask);
                }
            }
        }
    } else {
        if (cpu_is_occupied(owner, cpuid)) {
            CPU_SET(cpuid, &shdata->occupied_cores);
        } else {
            CPU_CLR(cpuid, &shdata->occupied_cores);
        }
    }
}

static pid_t find_new_guest(cpuinfo_t *cpuinfo) {
    pid_t new_guest = NOBODY;
    if (cpuinfo->state == CPU_BUSY) {
        /* If CPU is claimed, ignore requests and assign owner */
        new_guest = cpuinfo->owner;
    } else if (shdata->flags.queues_enabled) {
        /* Pop first PID in queue that is eligible for this CPU */
        for (pid_t *it = queue_pid_t_front(&cpuinfo->requests);
                it != NULL && new_guest == NOBODY;
                it = queue_pid_t_next(&cpuinfo->requests, it)) {
            if (core_is_eligible(*it, cpuinfo->id)) {
                new_guest = *it;
                queue_pid_t_delete(&cpuinfo->requests, it);
            }
        }

        /* If CPU did not have requests, pop global queue */
        if (new_guest == NOBODY) {
            for (lewi_mask_request_t *it =
                    queue_lewi_mask_request_t_front(&shdata->lewi_mask_requests);
                    it != NULL && new_guest == NOBODY;
                    it = queue_lewi_mask_request_t_next(&shdata->lewi_mask_requests, it)) {
                if (CPU_ISSET(cpuinfo->id, &it->allowed)
                        && core_is_eligible(it->pid, cpuinfo->id)) {
                    new_guest = it->pid;
                    if (--(it->howmany) == 0) {
                        queue_lewi_mask_request_t_delete(&shdata->lewi_mask_requests, it);
                    }
                }
            }
        }
    } else {
        /* No suitable guest */
        new_guest = NOBODY;
    }
    return new_guest;
}


/*********************************************************************************/
/*  Register / Deregister CPU                                                    */
/*********************************************************************************/

static void register_cpu(cpuinfo_t *cpuinfo, int pid, int preinit_pid) {

    /* Set basic fields */
    cpuinfo->owner = pid;
    cpuinfo->state = CPU_BUSY;
    if (cpuinfo->guest == NOBODY || cpuinfo->guest == preinit_pid) {
        cpuinfo->guest = pid;
    }
    CPU_CLR(cpuinfo->id, &shdata->free_cpus);

    /* Add or remove CPUs in core to the occupied cores set */
    update_occupied_cores(pid, cpuinfo->id);
}

static void deregister_cpu(cpuinfo_t *cpuinfo, int pid) {

    cpuid_t cpuid = cpuinfo->id;

    if (cpuinfo->owner == pid) {
        cpuinfo->owner = NOBODY;
        if (cpuinfo->guest == pid) {
            cpuinfo->guest = NOBODY;
        }
        if (cpu_is_public_post_mortem || !respect_cpuset) {
            cpuinfo->state = CPU_LENT;
            if (cpuinfo->guest == NOBODY) {
                CPU_SET(cpuid, &shdata->free_cpus);
            }
        } else {
            cpuinfo->state = CPU_DISABLED;
            queue_pid_t_clear(&cpuinfo->requests);
            CPU_CLR(cpuid, &shdata->free_cpus);
        }
        /* Clear all CPUs in core from the occupied */
        const cpu_set_t *core_mask = mu_get_core_mask(cpuinfo->id)->set;
        mu_subtract(&shdata->occupied_cores, &shdata->occupied_cores, core_mask);
    } else {
        // Free external CPUs that I may be using
        if (cpuinfo->guest == pid) {
            cpuinfo->guest = NOBODY;
            CPU_SET(cpuid, &shdata->free_cpus);
        }

        // Remove any previous CPU request
        if (shdata->flags.queues_enabled) {
            queue_pid_t_remove(&cpuinfo->requests, pid);
        }
    }
}


/*********************************************************************************/
/*  Init / Register                                                              */
/*********************************************************************************/

static void cleanup_shmem(void *shdata_ptr, int pid) {
    shdata_t *shared_data = shdata_ptr;
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        cpuinfo_t *cpuinfo = &shared_data->node_info[cpuid];
        deregister_cpu(cpuinfo, pid);
    }
}

static void open_shmem(const char *shmem_key, int shmem_color) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            node_size = mu_get_system_size();
            shm_handler = shmem_init((void**)&shdata,
                    &(const shmem_props_t) {
                        .size = shmem_cpuinfo__size(),
                        .name = shmem_name,
                        .key = shmem_key,
                        .color = shmem_color,
                        .version = SHMEM_CPUINFO_VERSION,
                        .cleanup_fn = cleanup_shmem,
                    });
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void init_shmem(void) {
    // Initialize some values if this is the 1st process attached to the shmem
    if (!shdata->flags.initialized) {
        shdata->flags = (const cpuinfo_flags_t) {
            .initialized = true,
            .hw_has_smt = mu_system_has_smt(),
        };
        get_time(&shdata->initial_time);
        shdata->timestamp_cpu_lent = 0;

        /* Initialize helper cpu sets */
        CPU_ZERO(&shdata->free_cpus);
        CPU_ZERO(&shdata->occupied_cores);

        /* Initialize global requests */
        queue_lewi_mask_request_t_init(&shdata->lewi_mask_requests);

        /* Initialize CPU ids */
        struct timespec now;
        get_time(&now);
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            shdata->node_info[cpuid] = (const cpuinfo_t) {
                .id = cpuid,
                .core_id = mu_get_core_id(cpuid),
            };

            /* Initialize cpuinfo queue */
            queue_pid_t_init(&shdata->node_info[cpuid].requests);

            /* If registered CPU set is not respected, all CPUs start as
             * available from the beginning */
            if (!respect_cpuset) {
                shdata->node_info[cpuid].state = CPU_LENT;
                CPU_SET(cpuid, &shdata->free_cpus);
            }
        }
    }
}

static int register_process(pid_t pid, pid_t preinit_pid, const cpu_set_t *mask, bool steal) {
    if (CPU_COUNT(mask) == 0) return DLB_SUCCESS;

    verbose(VB_SHMEM, "Registering process %d with mask %s", pid, mu_to_str(mask));

    if (!steal) {
        // Check first that my mask is not already owned
        for (int cpuid = mu_get_first_cpu(mask);
                cpuid >= 0 && cpuid < node_size;
                cpuid = mu_get_next_cpu(mask, cpuid)) {

            pid_t owner = shdata->node_info[cpuid].owner;

            if (owner != NOBODY && owner != pid && owner != preinit_pid) {
                verbose(VB_SHMEM,
                        "Error registering CPU %d, already owned by %d",
                        cpuid, owner);
                return DLB_ERR_PERM;
            }
        }
    }

    // Register mask
    for (int cpuid = mu_get_first_cpu(mask);
            cpuid >= 0 && cpuid < node_size;
            cpuid = mu_get_next_cpu(mask, cpuid)) {

        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

        if (steal && cpuinfo->owner != NOBODY && cpuinfo->owner != pid) {
            verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpuid);
        }

        register_cpu(cpuinfo, pid, preinit_pid);
    }

    return DLB_SUCCESS;
}

int shmem_cpuinfo__init(pid_t pid, pid_t preinit_pid, const cpu_set_t *process_mask,
        const char *shmem_key, int shmem_color) {
    int error = DLB_SUCCESS;

    // Determine if CPU should be public in post-mortem mode
    cpu_is_public_post_mortem = thread_spd &&
        (thread_spd->options.debug_opts & DBG_LPOSTMORTEM
         || !thread_spd->options.lewi_respect_cpuset);

    // Determine whether to respect the cpuset
    respect_cpuset = !(thread_spd && !thread_spd->options.lewi_respect_cpuset);

    // Shared memory creation
    open_shmem(shmem_key, shmem_color);

    //cpu_set_t affinity_mask;
    //mu_get_nodes_intersecting_with_cpuset(&affinity_mask, process_mask);

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        // Initialize shared memory, if needed
        init_shmem();

        // Register process_mask, with stealing = false always in normal Init()
        error = register_process(pid, preinit_pid, process_mask, /* steal */ false);
    }
    shmem_unlock(shm_handler);

    // TODO mask info should go in shmem_procinfo. Print something else here?
    //verbose( VB_SHMEM, "Process Mask: %s", mu_to_str(process_mask) );
    //verbose( VB_SHMEM, "Process Affinity Mask: %s", mu_to_str(&affinity_mask) );

    //add_event(IDLE_CPUS_EVENT, idle_count);

    if (error == DLB_ERR_PERM) {
        warn_error(DLB_ERR_PERM);
    }

    if (error != DLB_SUCCESS) {
        verbose(VB_SHMEM,
                "Error during shmem_cpuinfo initialization, finalizing shared memory");
        shmem_cpuinfo__finalize(pid, shmem_key, shmem_color);
    }

    return error;
}

int shmem_cpuinfo_ext__init(const char *shmem_key, int shmem_color) {
    open_shmem(shmem_key, shmem_color);
    return DLB_SUCCESS;
}

int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;
    int error;
    shmem_lock(shm_handler);
    {
        // Initialize shared memory, if needed
        init_shmem();

        // Register process_mask, with stealing according to user arguments
        error = register_process(pid, /* preinit_pid */ 0, mask, flags & DLB_STEAL_CPUS);
    }
    shmem_unlock(shm_handler);

    if (error == DLB_ERR_PERM) {
        warn_error(DLB_ERR_PERM);
    }

    return error;
}


/*********************************************************************************/
/*  Finalize / Deregister                                                        */
/*********************************************************************************/

static void close_shmem(void) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, is_shmem_empty);
            shm_handler = NULL;
            shdata = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

/* Even though the correct deregistration should be done through shmem_cpuinfo__deregister,
 * this function is kept to allow deregistration from outside the LeWI_mask policy */
static void deregister_process(pid_t pid) {
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        deregister_cpu(cpuinfo, pid);
    }

    // Remove any previous global request
    if (shdata->flags.queues_enabled) {
        queue_lewi_mask_request_t_remove(&shdata->lewi_mask_requests, pid);
    }
}

int shmem_cpuinfo__finalize(pid_t pid, const char *shmem_key, int shmem_color) {
    if (shm_handler == NULL) {
        /* cpuinfo_finalize may be called to finalize existing process
         * even if the file descriptor is not opened. (DLB_PreInit + forc-exec case) */
        if (shmem_exists(shmem_name, shmem_key)) {
            open_shmem(shmem_key, shmem_color);
        } else {
            return DLB_ERR_NOSHMEM;
        }
    }

    //DLB_INSTR( int idle_count = 0; )

    // Lock the shmem to deregister CPUs
    shmem_lock(shm_handler);
    {
        deregister_process(pid);
        //DLB_INSTR( if (is_idle(cpuid)) idle_count++; )
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    // Shared memory destruction
    close_shmem();

    //add_event(IDLE_CPUS_EVENT, idle_count);

    return DLB_SUCCESS;
}

int shmem_cpuinfo_ext__finalize(void) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    // Shared memory destruction
    close_shmem();

    return DLB_SUCCESS;
}

int shmem_cpuinfo_ext__postfinalize(pid_t pid) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        deregister_process(pid);
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    return error;
}


/*********************************************************************************/
/*  Lend CPU                                                                     */
/*********************************************************************************/

/* Add cpu_mask to the Shared Mask
 * If the process originally owns the CPU:      State => CPU_LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
static void lend_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (unlikely(cpuinfo->state == CPU_DISABLED)) return;

    if (cpuinfo->owner == pid) {
        // If the CPU is owned by the process, just change the state
        cpuinfo->state = CPU_LENT;
    } else if (shdata->flags.queues_enabled) {
        // Otherwise, remove any previous request
        queue_pid_t_remove(&cpuinfo->requests, pid);
    }

    // If the process is the guest, free it
    if (cpuinfo->guest == pid) {
        cpuinfo->guest = NOBODY;
    }

    // If the CPU is free, find a new guest
    if (cpuinfo->guest == NOBODY) {
        pid_t new_guest = find_new_guest(cpuinfo);
        if (new_guest != NOBODY) {
            cpuinfo->guest = new_guest;
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = new_guest,
                        .cpuid = cpuid,
                    });

            // If SMT is enabled, this CPU could have been the last lent
            // CPU in core, allowing find_new_guest to find new guests for
            // the rest of CPUs in the core. Iterate the rest of cpus now:
            const mu_cpuset_t *core_mask = mu_get_core_mask(cpuid);
            for (int cpuid_in_core = core_mask->first_cpuid;
                    cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
                    cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
                if (cpuid_in_core != cpuid) {
                    cpuinfo_t *cpuinfo_in_core = &shdata->node_info[cpuid_in_core];
                    if (cpuinfo_in_core->guest == NOBODY) {
                        new_guest = find_new_guest(cpuinfo_in_core);
                        if (new_guest != NOBODY) {
                            cpuinfo_in_core->guest = new_guest;
                            array_cpuinfo_task_t_push(
                                    tasks,
                                    (const cpuinfo_task_t) {
                                    .action = ENABLE_CPU,
                                    .pid = new_guest,
                                    .cpuid = cpuid_in_core,
                                    });
                            CPU_CLR(cpuid_in_core, &shdata->free_cpus);
                        }
                    }
                }
            }
        }
    }

    // Add CPU to the appropriate CPU sets
    if (cpuinfo->guest == NOBODY) {
        CPU_SET(cpuid, &shdata->free_cpus);
    }

    // Add or remove CPUs in core to the occupied cores set
    update_occupied_cores(cpuinfo->owner, cpuinfo->id);
}

int shmem_cpuinfo__lend_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    if (cpuid >= node_size) return DLB_ERR_PERM;

    int error = DLB_SUCCESS;
    //DLB_DEBUG( cpu_set_t freed_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    //DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        lend_cpu(pid, cpuid, tasks);

        //// Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
        //int i;
        //for (i = 0; i < node_size; i++) {
            //if (is_idle(i)) {
                //DLB_INSTR( idle_count++; )
                //DLB_DEBUG( CPU_SET(i, &idle_cpus); )
            //}
        //}
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    //DLB_DEBUG( int size = CPU_COUNT(&freed_cpus); )
    //DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    //verbose(VB_SHMEM, "Lending %s", mu_to_str(&freed_cpus));
    //verbose(VB_SHMEM, "Increasing %d Idle Threads (%d now)", size, post_size);
    //verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    //add_event(IDLE_CPUS_EVENT, idle_count);
    return error;
}

int shmem_cpuinfo__lend_cpu_mask(pid_t pid, const cpu_set_t *restrict mask,
        array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_SUCCESS;

    //DLB_DEBUG( cpu_set_t freed_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    //DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        for (int cpuid = mu_get_first_cpu(mask);
                cpuid >= 0 && cpuid < node_size;
                cpuid = mu_get_next_cpu(mask, cpuid)) {
            lend_cpu(pid, cpuid, tasks);

            //// Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            //if (is_idle(cpuid)) {
                //DLB_INSTR( idle_count++; )
                //DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            //}
        }
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    //DLB_DEBUG( int size = CPU_COUNT(&freed_cpus); )
    //DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    //verbose(VB_SHMEM, "Lending %s", mu_to_str(&freed_cpus));
    //verbose(VB_SHMEM, "Increasing %d Idle Threads (%d now)", size, post_size);
    //verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    //add_event(IDLE_CPUS_EVENT, idle_count);
    return error;
}


/*********************************************************************************/
/*  Reclaim CPU                                                                  */
/*********************************************************************************/

/* Recover CPU from the Shared Mask
 * CPUs that owner == ME:           State => CPU_BUSY
 * CPUs that guest == NOBODY        Guest => ME
 */
static int reclaim_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
    if (cpuinfo->owner == pid) {
        cpuinfo->state = CPU_BUSY;
        if (cpuinfo->guest == pid) {
            error = DLB_NOUPDT;
        }
        else if (cpuinfo->guest == NOBODY) {
            /* The CPU was idle, acquire it */
            cpuinfo->guest = pid;
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });
            CPU_CLR(cpuid, &shdata->free_cpus);
            error = DLB_SUCCESS;
        } else {
            /* The CPU was guested, reclaim it */
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = DISABLE_CPU,
                        .pid = cpuinfo->guest,
                        .cpuid = cpuid,
                    });
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });
            error = DLB_NOTED;
        }
    } else {
        error = DLB_ERR_PERM;
    }

    return error;
}

static int reclaim_core(pid_t pid, cpuid_t core_id,
        array_cpuinfo_task_t *restrict tasks,
        unsigned int *num_reclaimed) {

    int error = DLB_NOUPDT;
    *num_reclaimed = 0;

    const mu_cpuset_t *core_mask = mu_get_core_mask_by_coreid(core_id);
    for (int cpuid_in_core = core_mask->first_cpuid;
            cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
            cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
        int local_error = reclaim_cpu(pid, cpuid_in_core, tasks);
        if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
            ++(*num_reclaimed);
            if (error != DLB_NOTED) {
                error = local_error;
            }
        } else if (shdata->node_info[cpuid_in_core].guest == pid) {
            /* already guested, continue */
        } else {
            /* could not be borrowed for other reason, stop iterating */
            break;
        }
    }

    return error;
}

int shmem_cpuinfo__reclaim_all(pid_t pid, array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        cpu_set_t cpus_to_reclaim;
        CPU_OR(&cpus_to_reclaim, &shdata->free_cpus, &shdata->occupied_cores);

        for (int cpuid = mu_get_first_cpu(&cpus_to_reclaim);
                cpuid >= 0;
                cpuid = mu_get_next_cpu(&cpus_to_reclaim, cpuid)) {
            if (shdata->node_info[cpuid].owner == pid
                    && shdata->node_info[cpuid].guest != pid) {
                int local_error = reclaim_cpu(pid, cpuid, tasks);
                switch(local_error) {
                    case DLB_NOTED:
                        // max priority, always overwrite
                        error = DLB_NOTED;
                        break;
                    case DLB_SUCCESS:
                        // medium priority, only update if error is in lowest priority
                        error = (error == DLB_NOTED) ? DLB_NOTED : DLB_SUCCESS;
                        break;
                    case DLB_NOUPDT:
                        // lowest priority, default value
                        break;
                    case DLB_ERR_PERM:
                        // ignore
                        break;
                }
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__reclaim_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    if (cpuid >= node_size) return DLB_ERR_PERM;

    int error;
    //DLB_DEBUG( cpu_set_t recovered_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&recovered_cpus); )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        error = reclaim_cpu(pid, cpuid, tasks);

        /* If the CPU was actually reclaimed and SMT is enabled, the rest of
         * the CPUs in the core need to be disabled.
         * Note that we are not changing the CPU state to BUSY because the owner
         * still has not reclaim it. */
        if (error == DLB_NOTED
                && shdata->flags.hw_has_smt) {
            const mu_cpuset_t *core_mask = mu_get_core_mask(cpuid);
            for (int cpuid_in_core = core_mask->first_cpuid;
                    cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
                    cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
                if (cpuid_in_core != cpuid) {
                    const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid_in_core];
                    if (cpuinfo->guest != pid) {
                        array_cpuinfo_task_t_push(
                                tasks,
                                (const cpuinfo_task_t) {
                                    .action = DISABLE_CPU,
                                    .pid = cpuinfo->guest,
                                    .cpuid = cpuid_in_core,
                                });
                    }
                }
            }
        }

        // if (!error) //DLB_DEBUG( CPU_SET(cpu, &recovered_cpus); )

        // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
        //if (is_idle(cpu)) {
            //DLB_INSTR( idle_count++; )
            //DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
        //}
    }
    shmem_unlock(shm_handler);

    //DLB_DEBUG( int recovered = CPU_COUNT(&recovered_cpus); )
    //DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    //verbose(VB_SHMEM, "Decreasing %d Idle Threads (%d now)", recovered, post_size);
    //verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    //add_event(IDLE_CPUS_EVENT, idle_count);
    return error;
}

int shmem_cpuinfo__reclaim_cpus(pid_t pid, int ncpus, array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_NOUPDT;
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    //cpu_set_t recovered_cpus;
    //CPU_ZERO(&recovered_cpus);

    shmem_lock(shm_handler);
    {
        int num_cores = mu_get_num_cores();
        for (int core_id = 0; core_id < num_cores && ncpus>0; ++core_id) {
            unsigned int num_reclaimed;
            int local_error = reclaim_core(pid, core_id, tasks, &num_reclaimed);
            switch(local_error) {
                case DLB_NOTED:
                    // max priority, always overwrite
                    error = DLB_NOTED;
                    ncpus -= num_reclaimed;
                    break;
                case DLB_SUCCESS:
                    // medium priority, only update if error is in lowest priority
                    error = (error == DLB_NOTED) ? DLB_NOTED : DLB_SUCCESS;
                    ncpus -= num_reclaimed;
                    break;
                case DLB_NOUPDT:
                    // lowest priority, default value
                    break;
                case DLB_ERR_PERM:
                    // ignore
                    break;
            }
            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            //if (is_idle(cpu)) {
                //DLB_INSTR( idle_count++; )
                //DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            //}
        }
    }
    shmem_unlock(shm_handler);

    //DLB_DEBUG( int recovered = CPU_COUNT(&recovered_cpus); )
    //DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    //verbose(VB_SHMEM, "Decreasing %d Idle Threads (%d now)", recovered, post_size);
    //verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    //add_event(IDLE_CPUS_EVENT, idle_count);
    return error;
}

int shmem_cpuinfo__reclaim_cpu_mask(pid_t pid, const cpu_set_t *restrict mask,
        array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        cpu_set_t cpus_to_reclaim;
        CPU_OR(&cpus_to_reclaim, &shdata->free_cpus, &shdata->occupied_cores);
        CPU_AND(&cpus_to_reclaim, &cpus_to_reclaim, mask);

        for (int cpuid = mu_get_first_cpu(&cpus_to_reclaim);
                cpuid >= 0 && cpuid < node_size;
                cpuid = mu_get_next_cpu(&cpus_to_reclaim, cpuid)) {
            int local_error = reclaim_cpu(pid, cpuid, tasks);
            switch(local_error) {
                case DLB_ERR_PERM:
                    // max priority, always overwrite
                    error = DLB_ERR_PERM;
                    break;
                case DLB_NOTED:
                    // max priority unless there was a previous error
                    error = error < 0 ? error : DLB_NOTED;
                    break;
                case DLB_SUCCESS:
                    // medium priority, only update if error is in lowest priority
                    error = (error == DLB_NOUPDT) ? DLB_SUCCESS : error;
                    break;
                case DLB_NOUPDT:
                    // lowest priority, default value
                    break;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*  Acquire CPU                                                                  */
/*********************************************************************************/

/* Acquire CPU
 * If successful:       Guest => ME
 */
static int acquire_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->guest == pid) {
        // CPU already guested
        error = DLB_NOUPDT;
    } else if (cpuinfo->owner == pid) {
        // CPU is owned by the process
        cpuinfo->state = CPU_BUSY;
        if (cpuinfo->guest == NOBODY) {
            // CPU empty
            cpuinfo->guest = pid;
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });
            CPU_CLR(cpuid, &shdata->free_cpus);
            error = DLB_SUCCESS;
        } else {
            // CPU needs to be reclaimed
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = DISABLE_CPU,
                        .pid = cpuinfo->guest,
                        .cpuid = cpuid,
                    });
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });

            if (!CPU_ISSET(cpuid, &shdata->occupied_cores)) {
                update_occupied_cores(cpuinfo->owner, cpuinfo->id);
            }

            error = DLB_NOTED;
        }
    } else if (cpuinfo->guest == NOBODY
                && cpuinfo->state == CPU_LENT
                && core_is_eligible(pid, cpuid)) {
        // CPU is available
        cpuinfo->guest = pid;
        array_cpuinfo_task_t_push(
                tasks,
                (const cpuinfo_task_t) {
                    .action = ENABLE_CPU,
                    .pid = pid,
                    .cpuid = cpuid,
                });

        if (cpuinfo->owner != NOBODY
                && !CPU_ISSET(cpuid, &shdata->occupied_cores)) {
            update_occupied_cores(cpuinfo->owner, cpuinfo->id);
        }

        CPU_CLR(cpuid, &shdata->free_cpus);

        error = DLB_SUCCESS;
    } else if (shdata->flags.queues_enabled) {
        // CPU is not available, add request (async mode)
        if (queue_pid_t_enqueue(&cpuinfo->requests, pid) == 0) {
            error = DLB_NOTED;
        } else {
            error = DLB_ERR_REQST;
        }
    } else if (cpuinfo->state != CPU_DISABLED) {
        // CPU is busy, or lent to another process (polling mode)
        error = DLB_NOUPDT;
    } else {
        // CPU is disabled (polling mode)
        error = DLB_ERR_PERM;
    }

    return error;
}

static int acquire_core(pid_t pid, cpuid_t core_id,
        array_cpuinfo_task_t *restrict tasks,
        unsigned int *num_acquired) {

    int error = DLB_NOUPDT;
    *num_acquired = 0;

    const mu_cpuset_t *core_mask = mu_get_core_mask_by_coreid(core_id);
    for (int cpuid_in_core = core_mask->first_cpuid;
            cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
            cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
        int local_error = acquire_cpu(pid, cpuid_in_core, tasks);
        if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
            ++(*num_acquired);
            if (error != DLB_NOTED) {
                error = local_error;
            }
        } else if (shdata->node_info[cpuid_in_core].guest == pid) {
            /* already guested, continue */
        } else {
            /* could not be borrowed for other reason, stop iterating */
            break;
        }
    }

    return error;
}

static int acquire_cpus_in_array_cpuid_t(pid_t pid,
        const array_cpuid_t *restrict array_cpuid,
        int *restrict ncpus, array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_NOUPDT;
    int _ncpus = ncpus != NULL ? *ncpus : INT_MAX;
    const bool hw_has_smt = shdata->flags.hw_has_smt;

    /* Acquire all CPUs in core if possible
     * (there is a high chance that consecutive CPUs belong to the same core,
     * try to skip those ones) */
    int prev_core_id = -1;
    for (unsigned int i = 0;
            _ncpus > 0 && i < array_cpuid->count;
            ++i) {

        cpuid_t cpuid = array_cpuid->items[i];
        int local_error;

        if (hw_has_smt) {
            cpuid_t core_id = shdata->node_info[cpuid].core_id;
            if (prev_core_id != core_id) {
                unsigned int num_acquired;
                local_error = acquire_core(pid, core_id, tasks, &num_acquired);
                if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                    _ncpus -= num_acquired;
                    if (error != DLB_NOTED) error = local_error;
                }
                prev_core_id = core_id;
            }
        } else {
            local_error = acquire_cpu(pid, cpuid, tasks);
            if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                --_ncpus;
                if (error != DLB_NOTED) error = local_error;
            }
        }
    }

    if (ncpus != NULL) {
        *ncpus = _ncpus;
    }

    return error;
}

int shmem_cpuinfo__acquire_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    if (cpuid >= node_size) return DLB_ERR_PERM;

    int error;
    shmem_lock(shm_handler);
    {
        error = acquire_cpu(pid, cpuid, tasks);
    }
    shmem_unlock(shm_handler);
    return error;
}

/* Simplification of shmem_cpuinfo__acquire_ncpus_from_cpu_subset when we just
 * want to iterate all CPUs in set.
 * This function is intended to be called when leaving a blocking call and the
 * process knows that the provided CPUs were previously lent and need to be
 * reclaimed.
 *
 * PRE: all CPUS are owned */
int shmem_cpuinfo__acquire_from_cpu_subset(
        pid_t pid,
        const array_cpuid_t *restrict array_cpuid,
        array_cpuinfo_task_t *restrict tasks) {

    int error;
    shmem_lock(shm_handler);
    {
        error = acquire_cpus_in_array_cpuid_t(pid, array_cpuid, NULL, tasks);
    }
    shmem_unlock(shm_handler);
    return error;
}

static int borrow_cpus_in_array_cpuid_t(pid_t pid,
        const array_cpuid_t *restrict array_cpuid,
        int *restrict ncpus, array_cpuinfo_task_t *restrict tasks);

int shmem_cpuinfo__acquire_ncpus_from_cpu_subset(
        pid_t pid, int *restrict requested_ncpus,
        const array_cpuid_t *restrict cpus_priority_array,
        lewi_affinity_t lewi_affinity, int max_parallelism,
        int64_t *restrict last_borrow, array_cpuinfo_task_t *restrict tasks) {

    /* Return immediately if requested_ncpus is present and not greater than zero */
    if (requested_ncpus && *requested_ncpus <= 0) {
        return DLB_NOUPDT;
    }

    /* Return immediately if there is nothing left to acquire */
    /* 1) If the timestamp of the last unsuccessful borrow is newer than the last CPU lent */
    if (last_borrow && *last_borrow > DLB_ATOMIC_LD_ACQ(&shdata->timestamp_cpu_lent)) {
        /* 2) Unless there's an owned CPUs not guested, in that case we will acquire anyway */
        bool all_owned_cpus_are_guested = true;
        for (unsigned int i=0; i<cpus_priority_array->count; ++i) {
            cpuid_t cpuid = cpus_priority_array->items[i];
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            /* Iterate until the first not owned CPU */
            if (cpuinfo->owner != pid) break;
            if (cpuinfo->guest != pid) {
                /* This CPU is owned and not guested, no need to iterate anymore */
                all_owned_cpus_are_guested = false;
                break;
            }
        }
        if (all_owned_cpus_are_guested) {
            return DLB_NOUPDT;
        }
    }

    /* Return immediately if the process has reached the max_parallelism */
    if (max_parallelism != 0) {
        for (unsigned int i=0; i<cpus_priority_array->count; ++i) {
            cpuid_t cpuid = cpus_priority_array->items[i];
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->guest == pid) {
                --max_parallelism;
            }
        }
        if (max_parallelism <= 0) {
            return DLB_NOUPDT;
        }
    }

    /* Compute the max number of CPUs to acquire */
    int ncpus = requested_ncpus ? *requested_ncpus : node_size;
    if (max_parallelism > 0) {
        ncpus = min_int(ncpus, max_parallelism);
    }

    /* Arrays for temporary CPU priority (lazy initialized and always used within the lock) */
    static array_cpuid_t owned_idle = {};
    static array_cpuid_t owned_non_idle = {};
    static array_cpuid_t non_owned = {};

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        /* Lazy init first time, clear afterwards */
        if (likely(owned_idle.items != NULL)) {
            array_cpuid_t_clear(&owned_idle);
            array_cpuid_t_clear(&owned_non_idle);
            array_cpuid_t_clear(&non_owned);
        } else {
            array_cpuid_t_init(&owned_idle, node_size);
            array_cpuid_t_init(&owned_non_idle, node_size);
            array_cpuid_t_init(&non_owned, node_size);
        }

        /* Iterate cpus_priority_array and construct all sub-arrays */
        for (unsigned int i = 0; i < cpus_priority_array->count; ++i) {
            cpuid_t cpuid = cpus_priority_array->items[i];
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

            if (cpuinfo->state == CPU_DISABLED) continue;

            if (cpuinfo->owner == pid) {
                if (cpuinfo->guest == NOBODY) {
                    array_cpuid_t_push(&owned_idle, cpuid);
                } else if (cpuinfo->guest != pid) {
                    array_cpuid_t_push(&owned_non_idle, cpuid);
                }
            } else if (cpuinfo->guest == NOBODY) {
                array_cpuid_t_push(&non_owned, cpuid);
            }
        }

        /* Acquire first owned CPUs that are IDLE */
        if (array_cpuid_t_count(&owned_idle) > 0) {
            int local_error = acquire_cpus_in_array_cpuid_t(pid, &owned_idle, &ncpus, tasks);
            if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
            }
        }

        /* Acquire the rest of owned CPUs */
        if (array_cpuid_t_count(&owned_non_idle) > 0) {
            int local_error = acquire_cpus_in_array_cpuid_t(pid, &owned_non_idle, &ncpus, tasks);
            if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
            }
        }

        /* Borrow non-owned CPUs */
        if (array_cpuid_t_count(&non_owned) > 0) {
            int local_error = borrow_cpus_in_array_cpuid_t(pid, &non_owned, &ncpus, tasks);
            if (local_error == DLB_SUCCESS) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
            }
        }

        /* Add petition if asynchronous mode */
        if (shdata->flags.queues_enabled) {

            /* Add a global petition if a number of CPUs was requested and not fully allocated */
            if (requested_ncpus) {
                if (ncpus > 0) {
                    /* Construct a mask of allowed CPUs */
                    lewi_mask_request_t request = {
                        .pid = pid,
                        .howmany = ncpus,
                    };
                    CPU_ZERO(&request.allowed);
                    for (unsigned int i=0; i<cpus_priority_array->count; ++i) {
                        cpuid_t cpuid = cpus_priority_array->items[i];
                        CPU_SET(cpuid, &request.allowed);
                    }

                    /* Enqueue request */
                    verbose(VB_SHMEM, "Requesting %d CPUs more after acquiring", ncpus);
                    lewi_mask_request_t *it;
                    for (it = queue_lewi_mask_request_t_front(&shdata->lewi_mask_requests);
                            it != NULL;
                            it = queue_lewi_mask_request_t_next(&shdata->lewi_mask_requests, it)) {
                        if (it->pid == pid
                                && CPU_EQUAL(&request.allowed, &it->allowed)) {
                            /* update entry */
                            it->howmany += request.howmany;
                            error = DLB_NOTED;
                            break;
                        }
                    }
                    if (it == NULL) {
                        /* or add new entry */
                        if (queue_lewi_mask_request_t_enqueue(
                                    &shdata->lewi_mask_requests, request) == 0) {
                            error = DLB_NOTED;
                        } else {
                            error = DLB_ERR_REQST;
                        }
                    }
                }
            }

            /* Otherwise, add petitions to all CPUs that are either non-owned or disabled */
            else {
                for (unsigned int i = 0; i < cpus_priority_array->count; ++i) {
                    cpuid_t cpuid = cpus_priority_array->items[i];
                    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

                    if (cpuinfo->state == CPU_DISABLED
                            || (cpuinfo->owner != pid
                                && cpuinfo->guest != pid)) {
                        if (queue_pid_t_search(&cpuinfo->requests, pid) == NULL
                                && queue_pid_t_has_space(&cpuinfo->requests)) {
                            queue_pid_t_enqueue(&cpuinfo->requests, pid);
                            error = DLB_NOTED;
                        }
                    }
                }
            }
        }

        /* Update timestamp if borrow did not succeed */
        if (last_borrow != NULL && error != DLB_SUCCESS && error != DLB_NOTED) {
            *last_borrow = get_time_in_ns();
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*  Borrow CPU                                                                   */
/*********************************************************************************/

static int borrow_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
    if (unlikely(cpuinfo->state == CPU_DISABLED)) return DLB_ERR_PERM;

    int error = DLB_NOUPDT;

    if (cpuinfo->guest == NOBODY
            && core_is_eligible(pid, cpuid)) {
        if (cpuinfo->owner == pid) {
            // CPU is owned by the process
            cpuinfo->state = CPU_BUSY;
            cpuinfo->guest = pid;
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });
            error = DLB_SUCCESS;
            CPU_CLR(cpuid, &shdata->free_cpus);
        } else if (cpuinfo->state == CPU_LENT) {
            // CPU is available
            cpuinfo->guest = pid;
            array_cpuinfo_task_t_push(
                    tasks,
                    (const cpuinfo_task_t) {
                        .action = ENABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                    });
            error = DLB_SUCCESS;
            CPU_CLR(cpuid, &shdata->free_cpus);
            if (cpuinfo->owner != NOBODY
                    && !CPU_ISSET(cpuid, &shdata->occupied_cores)) {
                update_occupied_cores(cpuinfo->owner, cpuinfo->id);
            }
        }
    }

    return error;
}

static int borrow_core(pid_t pid, cpuid_t core_id, array_cpuinfo_task_t *restrict tasks,
        unsigned int *num_borrowed) {

    int error = DLB_NOUPDT;
    *num_borrowed = 0;

    const mu_cpuset_t *core_mask = mu_get_core_mask_by_coreid(core_id);
    for (int cpuid_in_core = core_mask->first_cpuid;
            cpuid_in_core >= 0 && cpuid_in_core != DLB_CPUID_INVALID;
            cpuid_in_core = mu_get_next_cpu(core_mask->set, cpuid_in_core)) {
        if (borrow_cpu(pid, cpuid_in_core, tasks) == DLB_SUCCESS) {
            /* successfully borrowed, continue */
            ++(*num_borrowed);
            error = DLB_SUCCESS;
        } else if (shdata->node_info[cpuid_in_core].guest == pid) {
            /* already guested, continue */
        } else {
            /* could not be borrowed for other reason, stop iterating */
            break;
        }
    }

    return error;
}

/* Iterate array_cpuid_t and borrow all possible CPUs */
static int borrow_cpus_in_array_cpuid_t(pid_t pid,
        const array_cpuid_t *restrict array_cpuid,
        int *restrict ncpus, array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_NOUPDT;
    int _ncpus = ncpus != NULL ? *ncpus : INT_MAX;
    const bool hw_has_smt = shdata->flags.hw_has_smt;

    /* Borrow all CPUs in core if possible
     * (there is a high chance that consecutive CPUs belong to the same core,
     * try to skip those ones) */
    int prev_core_id = -1;
    for (unsigned int i = 0;
            _ncpus > 0 && i < array_cpuid->count;
            ++i) {

        cpuid_t cpuid = array_cpuid->items[i];

        if (hw_has_smt) {
            cpuid_t core_id = shdata->node_info[cpuid].core_id;
            if (prev_core_id != core_id) {
                unsigned int num_borrowed;
                if (borrow_core(pid, core_id, tasks, &num_borrowed) == DLB_SUCCESS) {
                    _ncpus -= num_borrowed;
                    error = DLB_SUCCESS;
                }
                prev_core_id = core_id;
            }
        } else {
            if (borrow_cpu(pid, cpuid, tasks) == DLB_SUCCESS) {
                --_ncpus;
                error = DLB_SUCCESS;
            }
        }
    }

    if (ncpus != NULL) {
        *ncpus = _ncpus;
    }

    return error;
}

/* Iterate cpu_set_t and borrow all possible CPUs */
static int borrow_cpus_in_cpu_set_t(pid_t pid,
        const cpu_set_t *restrict cpu_set,
        int *restrict ncpus, array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_NOUPDT;
    int _ncpus = ncpus != NULL ? *ncpus : INT_MAX;
    const bool hw_has_smt = shdata->flags.hw_has_smt;

    /* Borrow all CPUs in core if possible
     * (there is a high chance that consecutive CPUs belong to the same core,
     * try to skip those ones) */
    int prev_core_id = -1;
    for (int cpuid = mu_get_first_cpu(cpu_set);
            cpuid >= 0 && cpuid < node_size &&_ncpus > 0;
            cpuid = mu_get_next_cpu(cpu_set, cpuid)) {

        if (hw_has_smt) {
            cpuid_t core_id = shdata->node_info[cpuid].core_id;
            if (prev_core_id != core_id) {
                unsigned int num_borrowed;
                if (borrow_core(pid, core_id, tasks, &num_borrowed) == DLB_SUCCESS) {
                    _ncpus -= num_borrowed;
                    error = DLB_SUCCESS;
                }
                prev_core_id = core_id;
            }
        } else {
            if (borrow_cpu(pid, cpuid, tasks) == DLB_SUCCESS) {
                --_ncpus;
                error = DLB_SUCCESS;
            }
        }
    }

    if (ncpus != NULL) {
        *ncpus = _ncpus;
    }

    return error;
}


int shmem_cpuinfo__borrow_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    if (cpuid >= node_size) return DLB_ERR_PERM;

    int error;
    shmem_lock(shm_handler);
    {
        error = borrow_cpu(pid, cpuid, tasks);
    }
    shmem_unlock(shm_handler);
    return error;
}

/* Simplification of shmem_cpuinfo__borrow_ncpus_from_cpu_subset when we just
 * want to iterate all CPUs in set.
 * This function is intended to be called when leaving a blocking call and the
 * process knows that the provided CPUs were previously lent and may try to
 * borrow again.
 */
int shmem_cpuinfo__borrow_from_cpu_subset(
        pid_t pid,
        const array_cpuid_t *restrict array_cpuid,
        array_cpuinfo_task_t *restrict tasks) {

    int error;
    shmem_lock(shm_handler);
    {
        error = borrow_cpus_in_array_cpuid_t(pid, array_cpuid, NULL, tasks);
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__borrow_ncpus_from_cpu_subset(
        pid_t pid, int *restrict requested_ncpus,
        const array_cpuid_t *restrict cpus_priority_array, lewi_affinity_t lewi_affinity,
        int max_parallelism, int64_t *restrict last_borrow,
        array_cpuinfo_task_t *restrict tasks) {

    /* Return immediately if requested_ncpus is present and not greater than zero */
    if (requested_ncpus && *requested_ncpus <= 0) {
        return DLB_NOUPDT;
    }

    /* Return immediately if the timestamp of the last unsuccessful borrow is
     * newer than the last CPU lent */
    if (last_borrow && *last_borrow > DLB_ATOMIC_LD_ACQ(&shdata->timestamp_cpu_lent)) {
        return DLB_NOUPDT;
    }

    /* Return immediately if the process has reached the max_parallelism */
    if (max_parallelism != 0) {
        for (unsigned int i=0; i<cpus_priority_array->count; ++i) {
            cpuid_t cpuid = cpus_priority_array->items[i];
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->guest == pid) {
                --max_parallelism;
            }
        }
        if (max_parallelism <= 0) {
            return DLB_NOUPDT;
        }
    }

    /* Compute the max number of CPUs to borrow */
    int ncpus = requested_ncpus ? *requested_ncpus : node_size;
    if (max_parallelism > 0) {
        ncpus = min_int(ncpus, max_parallelism);
    }

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        /* Skip borrow if no CPUs in the free_cpus mask */
        if (CPU_COUNT(&shdata->free_cpus) == 0) {
            ncpus = 0;
        }

        /* Borrow CPUs in the cpus_priority_array */
        if (borrow_cpus_in_array_cpuid_t(pid, cpus_priority_array, &ncpus, tasks) == DLB_SUCCESS) {
            error = DLB_SUCCESS;
        }

        /* Only if --priority=spread-ifempty, borrow CPUs if there are free NUMA nodes */
        if (lewi_affinity == LEWI_AFFINITY_SPREAD_IFEMPTY && ncpus > 0) {
            cpu_set_t free_nodes;
            mu_get_nodes_subset_of_cpuset(&free_nodes, &shdata->free_cpus);
            if (borrow_cpus_in_cpu_set_t(pid, &free_nodes, &ncpus, tasks) == DLB_SUCCESS) {
                error = DLB_SUCCESS;
            }
        }
    }
    shmem_unlock(shm_handler);

    /* Update timestamp if borrow did not succeed */
    if (last_borrow != NULL && error != DLB_SUCCESS) {
        *last_borrow = get_time_in_ns();
    }

    return error;
}


/*********************************************************************************/
/*  Return CPU                                                                   */
/*********************************************************************************/

/* Return CPU
 * Abandon CPU given that state == BUSY, owner != pid, guest == pid
 */
static int return_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner == pid
            || cpuinfo->guest != pid
            || (cpuinfo->state == CPU_LENT
                && core_is_eligible(pid, cpuid))) {
        return DLB_NOUPDT;
    }

    // Return CPU
    if (cpuinfo->state == CPU_BUSY) {
        cpuinfo->guest = cpuinfo->owner;
    } else {
        /* state is disabled or the core is not eligible */
        cpuinfo->guest = NOBODY;
        CPU_SET(cpuid, &shdata->free_cpus);
    }

    // Possibly clear CPU from occupies cores set
    update_occupied_cores(cpuinfo->owner, cpuinfo->id);

    // current subprocess to disable cpu
    array_cpuinfo_task_t_push(
            tasks,
            (const cpuinfo_task_t) {
                .action = DISABLE_CPU,
                .pid = pid,
                .cpuid = cpuid,
            });

    return DLB_SUCCESS;
}

int shmem_cpuinfo__return_all(pid_t pid, array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        for (int cpuid = mu_get_first_cpu(&shdata->occupied_cores);
                cpuid >= 0;
                cpuid = mu_get_next_cpu(&shdata->occupied_cores, cpuid)) {
            int local_error = return_cpu(pid, cpuid, tasks);
            switch(local_error) {
                case DLB_ERR_REQST:
                    // max priority, always overwrite
                    error = DLB_ERR_REQST;
                    break;
                case DLB_SUCCESS:
                    // medium priority, only update if error is in lowest priority
                    error = (error == DLB_NOUPDT) ? DLB_SUCCESS : error;
                    break;
                case DLB_NOUPDT:
                    // lowest priority, default value
                    break;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, array_cpuinfo_task_t *restrict tasks) {

    if (cpuid >= node_size) return DLB_ERR_PERM;

    int error;
    shmem_lock(shm_handler);
    {
        if (unlikely(shdata->node_info[cpuid].guest != pid)) {
            error = DLB_ERR_PERM;
        } else {
            error = return_cpu(pid, cpuid, tasks);
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask,
        array_cpuinfo_task_t *restrict tasks) {

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        cpu_set_t cpus_to_return;
        CPU_AND(&cpus_to_return, mask, &shdata->occupied_cores);

        for (int cpuid = mu_get_first_cpu(&cpus_to_return);
                cpuid >= 0;
                cpuid = mu_get_next_cpu(&cpus_to_return, cpuid)) {
            int local_error = return_cpu(pid, cpuid, tasks);
            error = (error < 0) ? error : local_error;
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

static inline void shmem_cpuinfo__return_async(pid_t pid, cpuid_t cpuid) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    ensure(cpuinfo->owner != pid
            && cpuinfo->guest == pid, "cpuinfo inconsistency in %s", __func__);

    /* 'cpuid' should only be a guested non-owned CPU */
    if (cpuinfo->state == CPU_BUSY) {
        cpuinfo->guest = cpuinfo->owner;
    } else {
        cpuinfo->guest = NOBODY;
        CPU_SET(cpuid, &shdata->free_cpus);
    }

    // Possibly clear CPU from occupies cores set
    update_occupied_cores(cpuinfo->owner, cpuinfo->id);

    /* Add another CPU request */
    queue_pid_t_enqueue(&cpuinfo->requests, pid);
}

/* Only for asynchronous mode. This function is intended to be called after
 * a disable_cpu callback.
 * This function resolves returned CPUs, fixes guest and add a new request */
void shmem_cpuinfo__return_async_cpu(pid_t pid, cpuid_t cpuid) {

    shmem_lock(shm_handler);
    {
        shmem_cpuinfo__return_async(pid, cpuid);
    }
    shmem_unlock(shm_handler);
}

/* Only for asynchronous mode. This is function is intended to be called after
 * a disable_cpu callback.
 * This function resolves returned CPUs, fixes guest and add a new request */
void shmem_cpuinfo__return_async_cpu_mask(pid_t pid, const cpu_set_t *mask) {

    shmem_lock(shm_handler);
    {
        for (int cpuid = mu_get_first_cpu(mask);
                cpuid >= 0 && cpuid < node_size;
                cpuid = mu_get_next_cpu(mask, cpuid)) {

            shmem_cpuinfo__return_async(pid, cpuid);
        }
    }
    shmem_unlock(shm_handler);
}


/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/

/* Called when lewi_mask_finalize.
 * This function deregisters pid, disabling or lending CPUs as needed */
int shmem_cpuinfo__deregister(pid_t pid, array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        // Remove any request before acquiring and lending
        if (shdata->flags.queues_enabled) {
            queue_lewi_mask_request_t_remove(&shdata->lewi_mask_requests, pid);
            for (int cpuid=0; cpuid<node_size; ++cpuid) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                if (cpuinfo->owner != pid) {
                    queue_pid_t_remove(&cpuinfo->requests, pid);
                }
            }
        }

        // Iterate again to properly treat each CPU
        for (int cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                if (cpu_is_public_post_mortem || !respect_cpuset) {
                    /* Lend if public */
                    lend_cpu(pid, cpuid, tasks);
                } else {
                    /* If CPU won't be public, it must be reclaimed beforehand */
                    reclaim_cpu(pid, cpuid, tasks);
                    if (cpuinfo->guest == pid) {
                        cpuinfo->guest = NOBODY;
                    }
                    cpuinfo->state = CPU_DISABLED;
                    CPU_CLR(cpuid, &shdata->free_cpus);
                }
                cpuinfo->owner = NOBODY;

                /* It will be consistent as long as one core belongs to one process only */
                CPU_CLR(cpuid, &shdata->occupied_cores);
            } else {
                // Free external CPUs that I might be using
                if (cpuinfo->guest == pid) {
                    lend_cpu(pid, cpuid, tasks);
                }
            }
        }
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    return error;
}

/* Called when DLB_disable.
 * This function resets the initial status of pid: acquire owned, lend guested */
int shmem_cpuinfo__reset(pid_t pid, array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        // Remove any request before acquiring and lending
        if (shdata->flags.queues_enabled) {
            queue_lewi_mask_request_t_remove(&shdata->lewi_mask_requests, pid);
            for (int cpuid=0; cpuid<node_size; ++cpuid) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                if (cpuinfo->owner != pid) {
                    queue_pid_t_remove(&cpuinfo->requests, pid);
                }
            }
        }

        // Iterate again to properly reset each CPU
        for (int cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                reclaim_cpu(pid, cpuid, tasks);
            } else if (cpuinfo->guest == pid) {
                lend_cpu(pid, cpuid, tasks);
                array_cpuinfo_task_t_push(
                        tasks,
                        (const cpuinfo_task_t) {
                            .action = DISABLE_CPU,
                            .pid = pid,
                            .cpuid = cpuid,
                        });
            }
        }
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    return error;
}

/* Lend as many CPUs as needed to only guest as much as 'max' CPUs */
int shmem_cpuinfo__update_max_parallelism(pid_t pid, unsigned int max,
        array_cpuinfo_task_t *restrict tasks) {
    int error = DLB_SUCCESS;
    unsigned int owned_count = 0;
    unsigned int guested_count = 0;
    SMALL_ARRAY(cpuid_t, guested_cpus, node_size);
    shmem_lock(shm_handler);
    {
        for (cpuid_t cpuid=0; cpuid<node_size; ++cpuid) {
            const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                ++owned_count;
                if (max < owned_count) {
                    // Lend owned CPUs if the number of owned is greater than max
                    lend_cpu(pid, cpuid, tasks);
                    array_cpuinfo_task_t_push(
                            tasks,
                            (const cpuinfo_task_t) {
                                .action = DISABLE_CPU,
                                .pid = pid,
                                .cpuid = cpuid,
                            });
                }
            } else if (cpuinfo->guest == pid) {
                // Since owned_count is still unknown, just save our guested CPUs
                guested_cpus[guested_count++] = cpuid;
            }
        }

        // Iterate guested CPUs to lend them if needed
        for (unsigned int i=0; i<guested_count; ++i) {
            if (max < owned_count + i + 1) {
                cpuid_t cpuid = guested_cpus[i];
                lend_cpu(pid, cpuid, tasks);
                array_cpuinfo_task_t_push(
                        tasks,
                        (const cpuinfo_task_t) {
                        .action = DISABLE_CPU,
                        .pid = pid,
                        .cpuid = cpuid,
                        });
            }
        }
    }
    shmem_unlock(shm_handler);

    update_shmem_timestamp();

    return error;
}

/* Update CPU ownership according to the new process mask.
 * To avoid collisions, we only release the ownership if we still own it
 */
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t *restrict process_mask,
        array_cpuinfo_task_t *restrict tasks) {

    verbose(VB_SHMEM, "Updating ownership: %s", mu_to_str(process_mask));

    shmem_lock(shm_handler);

    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (CPU_ISSET(cpuid, process_mask)) {
            // The CPU should be mine
            if (cpuinfo->owner != pid) {
                // Not owned: Steal CPU
                cpuinfo->owner = pid;
                cpuinfo->state = CPU_BUSY;
                if (cpuinfo->guest == NOBODY) {
                    cpuinfo->guest = pid;
                    CPU_CLR(cpuid, &shdata->free_cpus);
                    CPU_CLR(cpuid, &shdata->occupied_cores);
                }
                if (tasks) {
                    if (cpuinfo->guest != pid) {
                        array_cpuinfo_task_t_push(
                                tasks,
                                (const cpuinfo_task_t) {
                                    .action = DISABLE_CPU,
                                    .pid = cpuinfo->guest,
                                    .cpuid = cpuid,
                                });
                    }
                    array_cpuinfo_task_t_push(
                            tasks,
                            (const cpuinfo_task_t) {
                                .action = ENABLE_CPU,
                                .pid = pid,
                                .cpuid = cpuid,
                            });
                }
                verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpuid);
            } else {
                // The CPU was already owned, no update needed
            }

        } else if (cpuid < node_size) {
            // The CPU should not be mine
            if (cpuinfo->owner == pid) {
                // Previously owned: Release CPU ownership
                cpuinfo->owner = NOBODY;
                cpuinfo->state = CPU_DISABLED;
                if (cpuinfo->guest == pid ) {
                    cpuinfo->guest = NOBODY;
                    if (tasks) {
                        array_cpuinfo_task_t_push(
                                tasks,
                                (const cpuinfo_task_t) {
                                    .action = DISABLE_CPU,
                                    .pid = pid,
                                    .cpuid = cpuid,
                                });
                    }
                    CPU_CLR(cpuid, &shdata->free_cpus);
                    verbose(VB_SHMEM, "Releasing ownership of CPU %d", cpuid);
                }
            } else {
                if (cpuinfo->guest == pid
                        && cpuinfo->state == CPU_BUSY) {
                    /* 'tasks' may be NULL if LeWI is disabled, but if the process
                     * is guesting an external CPU, LeWI should be enabled */
                    if (unlikely(tasks == NULL)) {
                        shmem_unlock(shm_handler);
                        fatal("tasks pointer is NULL in %s. Please report bug at %s",
                                __func__, PACKAGE_BUGREPORT);
                    }
                    // The CPU has been either stolen or reclaimed,
                    // return it anyway
                    return_cpu(pid, cpuid, tasks);
                }
            }
        }
    }

    shmem_unlock(shm_handler);
}

int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num) {
    if (unlikely(shm_handler == NULL || thread_num < 0)) return -1;

    SMALL_ARRAY(cpuid_t, guested_cpus, node_size);
    int owned_count = 0;
    int guested_count = 0;

    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (cpuinfo->owner == pid
                && cpuinfo->state == CPU_BUSY) {
            ++owned_count;
            if (thread_num < owned_count) {
                return cpuid;
            }
        } else if (cpuinfo->guest == pid) {
            guested_cpus[guested_count++] = cpuid;
        }
    }

    int binding;
    if (thread_num - owned_count < guested_count) {
        binding = guested_cpus[thread_num-owned_count];
    } else {
        binding = -1;
    }
    return binding;
}

/* Find the nth non owned CPU for a given PID.
 * The count always starts from the first owned CPU.
 * ex: process has mask [4,7] in a system mask [0-7],
 *         1st CPU (id=0) is 0
 *         4th CPU (id=3) is 3
 *         id > 3 -> -1
 */
int shmem_cpuinfo__get_nth_non_owned_cpu(pid_t pid, int nth_cpu) {
    int idx = 0;
    int owned_cpus = 0;
    int non_owned_cpus = 0;
    SMALL_ARRAY(cpuid_t, non_owned_cpu_list, node_size);

    /* Construct non owned CPU list */
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (cpuinfo->owner == pid) {
            if (owned_cpus++ == 0) {
                idx = non_owned_cpus;
            }
        } else if (cpuinfo->state != CPU_DISABLED) {
            non_owned_cpu_list[non_owned_cpus++] = cpuid;
        }
    }

    /* Find the nth element starting from the first owned CPU */
    if (nth_cpu < non_owned_cpus) {
        idx = (idx + nth_cpu) % non_owned_cpus;
        return non_owned_cpu_list[idx];
    } else {
        return -1;
    }
}

/* Return the number of registered CPUs not owned by the given PID */
int shmem_cpuinfo__get_number_of_non_owned_cpus(pid_t pid) {
    int num_non_owned_cpus = 0;
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        const cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (cpuinfo->owner != pid
                && cpuinfo->state != CPU_DISABLED) {
            ++num_non_owned_cpus;
        }
    }
    return num_non_owned_cpus;
}

int shmem_cpuinfo__check_cpu_availability(pid_t pid, int cpuid) {
    int error = DLB_NOTED;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner != pid
            && (cpuinfo->state == CPU_BUSY || cpuinfo->state == CPU_DISABLED) ) {
        /* The CPU is reclaimed or disabled */
        error = DLB_ERR_PERM;
    } else if (cpuinfo->guest == pid) {
        /* The CPU is already guested by the process */
        error = DLB_SUCCESS;
    } else if (cpuinfo->guest == NOBODY ) {
        /* Assign new guest if the CPU is empty */
        shmem_lock(shm_handler);
        {
            if (cpuinfo->guest == NOBODY) {
                cpuinfo->guest = pid;
                CPU_CLR(cpuid, &shdata->free_cpus);
                error = DLB_SUCCESS;
            }
        }
        shmem_unlock(shm_handler);
    } else if (cpuinfo->owner == pid
            && cpuinfo->state == CPU_LENT) {
        /* The owner is asking for a CPU not reclaimed yet */
        error = DLB_NOUPDT;
    }

    return error;
}

int shmem_cpuinfo__is_cpu_enabled(int cpuid) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
    return cpuinfo->state != CPU_DISABLED;
}

bool shmem_cpuinfo__exists(void) {
    return shm_handler != NULL;
}

void shmem_cpuinfo__enable_request_queues(void) {
    if (shm_handler == NULL) return;

    /* Enable asynchronous request queues */
    shdata->flags.queues_enabled = true;
}

void shmem_cpuinfo__remove_requests(pid_t pid) {
    if (shm_handler == NULL) return;
    shmem_lock(shm_handler);
    {
        /* Remove any previous request for the specific pid */
        if (shdata->flags.queues_enabled) {
            /* Remove global requests (pair <pid,howmany>) */
            queue_lewi_mask_request_t_remove(&shdata->lewi_mask_requests, pid);

            /* Remove specific CPU requests */
            int cpuid;
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                queue_pid_t_remove(&cpuinfo->requests, pid);
            }
        }
    }
    shmem_unlock(shm_handler);
}

int shmem_cpuinfo__version(void) {
    return SHMEM_CPUINFO_VERSION;
}

size_t shmem_cpuinfo__size(void) {
    return sizeof(shdata_t) + sizeof(cpuinfo_t)*mu_get_system_size();
}

void shmem_cpuinfo__print_info(const char *shmem_key, int shmem_color, int columns,
        dlb_printshmem_flags_t print_flags) {

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_cpuinfo_ext__init(shmem_key, shmem_color);
    }

    /* Make a full copy of the shared memory */
    shdata_t *shdata_copy = malloc(sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size);
    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size);
    }
    shmem_unlock(shm_handler);

    /* Close shmem if needed */
    if (temporary_shmem) {
        shmem_cpuinfo_ext__finalize();
    }

    /* Find the largest pid registered in the shared memory */
    pid_t max_pid = 0;
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        pid_t pid = shdata_copy->node_info[cpuid].owner;
        max_pid = pid > max_pid ? pid : max_pid;
    }
    int max_digits = snprintf(NULL, 0, "%d", max_pid);

    /* Do not print shared memory if nobody is registered */
    if (max_pid == 0) {
        free(shdata_copy);
        return;
    }

    /* Set up color */
    bool is_tty = isatty(STDOUT_FILENO);
    bool color = print_flags & DLB_COLOR_ALWAYS
        || (print_flags & DLB_COLOR_AUTO && is_tty);

    /* Set up number of columns */
    if (columns <= 0) {
        unsigned short width;
        if (is_tty) {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            width = w.ws_col ? w.ws_col : 80;
        } else {
            width = 80;
        }
        if (color) {
            columns = width / (13+max_digits*2);
        } else {
            columns = width / (20+max_digits*2);
        }
    }

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up line buffer */
    enum { MAX_LINE_LEN = 512 };
    char line[MAX_LINE_LEN];
    char *l;

    /* Calculate number of rows and cpus per column (same) */
    int rows = (node_size+columns-1) / columns;
    int cpus_per_column = rows;

    /* Update flag here in case this is an external process */
    if (thread_spd && !thread_spd->options.lewi_respect_cpuset) {
        respect_cpuset = false;
    }

    int row;
    for (row=0; row<rows; ++row) {
        /* Init line */
        line[0] = '\0';
        l = line;

        /* Iterate columns */
        int column;
        for (column=0; column<columns; ++column) {
            cpuid = row + column*cpus_per_column;
            if (cpuid < node_size) {
                const cpuinfo_t *cpuinfo = &shdata_copy->node_info[cpuid];
                pid_t owner = cpuinfo->owner;
                pid_t guest = cpuinfo->guest;
                cpu_state_t state = cpuinfo->state;
                if (color) {
                    const char *code_color =
                        state == CPU_DISABLED && respect_cpuset ? ANSI_COLOR_RESET :
                        state == CPU_BUSY && guest == owner     ? ANSI_COLOR_RED :
                        state == CPU_BUSY                       ? ANSI_COLOR_YELLOW :
                        guest == NOBODY                         ? ANSI_COLOR_GREEN :
                                                                  ANSI_COLOR_BLUE;
                    l += snprintf(l, MAX_LINE_LEN-strlen(line),
                            " %4d %s[ %*d / %*d ]" ANSI_COLOR_RESET,
                            cpuid,
                            code_color,
                            max_digits, owner,
                            max_digits, guest);
                } else {
                    const char *state_desc =
                        state == CPU_DISABLED               ? " off" :
                        state == CPU_BUSY && guest == owner ? "busy" :
                        state == CPU_BUSY                   ? "recl" :
                        guest == NOBODY                     ? "idle" :
                                                              "lent";
                    l += snprintf(l, MAX_LINE_LEN-strlen(line),
                            " %4d [ %*d / %*d / %s ]",
                            cpuid,
                            max_digits, owner,
                            max_digits, guest,
                            state_desc);
                }
            }
        }
        printbuffer_append(&buffer, line);
    }

    /* Print format */
    snprintf(line, MAX_LINE_LEN,
            "  Format: <cpuid> [ <owner> / <guest> %s]", color ? "" : "/ <state> ");
    printbuffer_append(&buffer, line);

    /* Print color legend */
    if (color) {
        snprintf(line, MAX_LINE_LEN,
                "  Status: Disabled, "
                ANSI_COLOR_RED "Owned" ANSI_COLOR_RESET ", "
                ANSI_COLOR_YELLOW "Reclaimed" ANSI_COLOR_RESET ", "
                ANSI_COLOR_GREEN "Idle" ANSI_COLOR_RESET ", "
                ANSI_COLOR_BLUE "Lent" ANSI_COLOR_RESET);
        printbuffer_append(&buffer, line);
    }

    /* CPU requests */
    bool any_cpu_request = false;
    for (cpuid=0; cpuid<node_size && !any_cpu_request; ++cpuid) {
        any_cpu_request = queue_pid_t_size(&shdata_copy->node_info[cpuid].requests) > 0;
    }
    if (any_cpu_request) {
        snprintf(line, MAX_LINE_LEN, "\n  CPU requests (<cpuid>: <spids>):");
        printbuffer_append(&buffer, line);
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            queue_pid_t *requests = &shdata_copy->node_info[cpuid].requests;
            if (queue_pid_t_size(requests) > 0) {
                /* Set up line */
                line[0] = '\0';
                l = line;
                l += snprintf(l, MAX_LINE_LEN-strlen(line), "  %4d: ", cpuid);
                /* Iterate requests */
                for (pid_t *it = queue_pid_t_front(requests);
                        it != NULL;
                        it = queue_pid_t_next(requests, it)) {
                    l += snprintf(l, MAX_LINE_LEN-strlen(line), " %u,", *it);
                }
                /* Remove trailing comma and append line */
                *(l-1) = '\0';
                printbuffer_append(&buffer, line);
            }
        }
    }

    /* Proc requests */
    if (queue_lewi_mask_request_t_size(&shdata_copy->lewi_mask_requests) > 0) {
        snprintf(line, MAX_LINE_LEN,
                "\n  Process requests (<spids>: <howmany>, <allowed_cpus>):");
        printbuffer_append(&buffer, line);
    }
    for (lewi_mask_request_t *it =
            queue_lewi_mask_request_t_front(&shdata_copy->lewi_mask_requests);
            it != NULL;
            it = queue_lewi_mask_request_t_next(&shdata_copy->lewi_mask_requests, it)) {
        snprintf(line, MAX_LINE_LEN,
                "    %*d: %d, %s",
                max_digits, it->pid, it->howmany, mu_to_str(&it->allowed));
        printbuffer_append(&buffer, line);
    }

    info0("=== CPU States ===\n%s", buffer.addr);
    printbuffer_destroy(&buffer);
    free(shdata_copy);
}

int shmem_cpuinfo_testing__get_num_proc_requests(void) {
    return shdata->flags.queues_enabled ?
        queue_lewi_mask_request_t_size(&shdata->lewi_mask_requests) : 0;
}

int shmem_cpuinfo_testing__get_num_cpu_requests(int cpuid) {
    return shdata->flags.queues_enabled ?
        queue_pid_t_size(&shdata->node_info[cpuid].requests) : 0;
}

const cpu_set_t* shmem_cpuinfo_testing__get_free_cpu_set(void) {
    return &shdata->free_cpus;
}

const cpu_set_t* shmem_cpuinfo_testing__get_occupied_core_set(void) {
    return &shdata->occupied_cores;
}

/*** Helper functions, the shm lock must have been acquired beforehand ***/
static inline bool is_idle(int cpu) {
    return shdata->node_info[cpu].state == CPU_LENT && shdata->node_info[cpu].guest == NOBODY;
}

static inline bool is_borrowed(pid_t pid, int cpu) {
    return shdata->node_info[cpu].state == CPU_BUSY && shdata->node_info[cpu].owner == pid;
}

static inline bool is_shmem_empty(void) {
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        if (shdata->node_info[cpuid].owner != NOBODY) {
            return false;
        }
    }
    return true;
}

/*** End of helper functions ***/
