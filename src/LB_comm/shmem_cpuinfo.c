/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* NOTE on default values:
 * The shared memory will be initializated to 0 when created,
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
    int             id;
    pid_t           owner;                  // Current owner
    pid_t           guest;                  // Current user of the CPU
    cpu_state_t     state;
    stats_state_t   stats_state;
    int64_t         acc_time[_NUM_STATS];   // Accumulated time for each state
    struct          timespec last_update;
    queue_pids_t    requests;
} cpuinfo_t;

typedef struct {
    struct              timespec initial_time;
    int64_t             timestamp_cpu_lent;
    bool                queues_enabled;
    queue_proc_reqs_t   proc_requests;
    cpuinfo_t           node_info[0];
} shdata_t;

enum { SHMEM_CPUINFO_VERSION = 5 };

static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int node_size;
static bool cpu_is_public_post_mortem = false;
static const char *shmem_name = "cpuinfo";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static inline bool is_idle(int cpu) __attribute__((unused));
static inline bool is_borrowed(pid_t pid, int cpu) __attribute__((unused));
static inline bool is_shmem_empty(void);
static void update_cpu_stats(int cpu, stats_state_t new_state);
static float getcpustate(int cpu, stats_state_t state, shdata_t *shared_data);


static pid_t find_new_guest(cpuinfo_t *cpuinfo) {
    pid_t new_guest;
    if (cpuinfo->state == CPU_BUSY) {
        /* If CPU is claimed, ignore requests an assign owner */
        new_guest = cpuinfo->owner;
    } else if (shdata->queues_enabled) {
        /* Pop first in CPU queue */
        queue_pids_pop(&cpuinfo->requests, &new_guest);

        /* If CPU did noy have requests, pop global queue */
        if (new_guest == NOBODY) {
            queue_proc_reqs_get(&shdata->proc_requests, &new_guest, cpuinfo->id);
        }
    } else {
        /* No suitable guest */
        new_guest = NOBODY;
    }
    return new_guest;
}

static void initialize_output_array(pid_t *array) {
    int i;
    for (i=0; i<node_size; ++i) {
        array[i] = -1;
    }
}

/*********************************************************************************/
/*  Init / Register                                                              */
/*********************************************************************************/

static void open_shmem(const char *shmem_key) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            node_size = mu_get_system_size();
            shm_handler = shmem_init((void**)&shdata,
                    sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size,
                    shmem_name, shmem_key, SHMEM_CPUINFO_VERSION);
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void init_shmem() {
    // Initialize some values if this is the 1st process attached to the shmem
    if (shdata->initial_time.tv_sec == 0 && shdata->initial_time.tv_nsec == 0) {
        get_time(&shdata->initial_time);
        shdata->timestamp_cpu_lent = 0;

        struct timespec now;
        get_time(&now);
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            shdata->node_info[cpuid].id = cpuid;
            shdata->node_info[cpuid].last_update = now;
        }
    }
}

static int register_process(pid_t pid, const cpu_set_t *mask, bool steal) {
    if (CPU_COUNT(mask) == 0) return DLB_SUCCESS;

    verbose(VB_SHMEM, "Registering process %d with mask %s", pid, mu_to_str(mask));

    int cpuid;
    if (!steal) {
        // Check first that my mask is not already owned
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                pid_t owner = shdata->node_info[cpuid].owner;
                if (owner != NOBODY && owner != pid) {
                    verbose(VB_SHMEM,
                            "Error registering CPU %d, already owned by %d",
                            cpuid, owner);
                    return DLB_ERR_PERM;
                }
            }
        }
    }

    // Register mask
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        if (CPU_ISSET(cpuid, mask)) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (steal && cpuinfo->owner != NOBODY && cpuinfo->owner != pid) {
                verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpuid);
            }
            if (cpuinfo->guest == NOBODY) {
                cpuinfo->guest = pid;
            }
            cpuinfo->owner = pid;
            cpuinfo->state = CPU_BUSY;
            update_cpu_stats(cpuid, STATS_OWNED);
        }
    }

    return DLB_SUCCESS;
}

int shmem_cpuinfo__init(pid_t pid, const cpu_set_t *process_mask, const char *shmem_key) {
    int error = DLB_SUCCESS;

    // Update post_mortem preference
    if (thread_spd && thread_spd->options.debug_opts & DBG_LPOSTMORTEM) {
        cpu_is_public_post_mortem = true;
    }

    // Shared memory creation
    open_shmem(shmem_key);

    //cpu_set_t affinity_mask;
    //mu_get_parents_covering_cpuset(&affinity_mask, process_mask);

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        // Initialize shared memory, if needed
        init_shmem();

        // Register process_mask, with stealing = false always in normal Init()
        error = register_process(pid, process_mask, false);
    }
    shmem_unlock(shm_handler);

    // TODO mask info should go in shmem_procinfo. Print something else here?
    //verbose( VB_SHMEM, "Process Mask: %s", mu_to_str(process_mask) );
    //verbose( VB_SHMEM, "Process Affinity Mask: %s", mu_to_str(&affinity_mask) );

    //add_event(IDLE_CPUS_EVENT, idle_count);

    if (error != DLB_SUCCESS) {
        verbose(VB_SHMEM,
                "Error during shmem_cpuinfo initialization, finalizing shared memory");
        shmem_cpuinfo__finalize(pid, shmem_key);
    }

    return error;
}

int shmem_cpuinfo_ext__init(const char *shmem_key) {
    open_shmem(shmem_key);
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
        error = register_process(pid, mask, flags & DLB_STEAL_CPUS);
    }
    shmem_unlock(shm_handler);

    return error;
}


/*********************************************************************************/
/*  Finalize / Deregister                                                        */
/*********************************************************************************/

static void close_shmem(bool shmem_empty) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, shmem_empty ? SHMEM_DELETE : SHMEM_NODELETE);
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
        if (cpuinfo->owner == pid) {
            cpuinfo->owner = NOBODY;
            if (cpuinfo->guest == pid) {
                cpuinfo->guest = NOBODY;
                update_cpu_stats(cpuid, STATS_IDLE);
            }
            if (cpu_is_public_post_mortem) {
                cpuinfo->state = CPU_LENT;
                shdata->timestamp_cpu_lent = get_time_in_ns();
            } else {
                cpuinfo->state = CPU_DISABLED;
            }
        } else {
            // Free external CPUs that I may be using
            if (cpuinfo->guest == pid) {
                cpuinfo->guest = NOBODY;
                update_cpu_stats(cpuid, STATS_IDLE);
            }

            // Remove any previous CPU request
            if (shdata->queues_enabled) {
                queue_pids_remove(&cpuinfo->requests, pid);
            }
        }
    }

    // Remove any previous global request
    if (shdata->queues_enabled) {
        queue_proc_reqs_remove(&shdata->proc_requests, pid);
    }
}

int shmem_cpuinfo__finalize(pid_t pid, const char *shmem_key) {
    if (shm_handler == NULL) {
        /* cpuinfo_finalize may be called to finalize existing process
         * even if the file descriptor is not opened. (DLB_PreInit + forc-exec case) */
        if (shmem_exists(shmem_name, shmem_key)) {
            open_shmem(shmem_key);
        } else {
            return DLB_ERR_NOSHMEM;
        }
    }

    //DLB_INSTR( int idle_count = 0; )

    bool shmem_empty;

    // Lock the shmem to deregister CPUs
    shmem_lock(shm_handler);
    {
        deregister_process(pid);
        shmem_empty = is_shmem_empty();
        //DLB_INSTR( if (is_idle(cpuid)) idle_count++; )
    }
    shmem_unlock(shm_handler);

    // Shared memory destruction
    close_shmem(shmem_empty);

    //add_event(IDLE_CPUS_EVENT, idle_count);

    return DLB_SUCCESS;
}

int shmem_cpuinfo_ext__finalize(void) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    bool shmem_empty;
    shmem_lock(shm_handler);
    {
        shmem_empty = is_shmem_empty();
    }
    shmem_unlock(shm_handler);

    close_shmem(shmem_empty);

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
    return error;
}


/*********************************************************************************/
/*  Lend CPU                                                                     */
/*********************************************************************************/

/* Add cpu_mask to the Shared Mask
 * If the process originally owns the CPU:      State => CPU_LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
static void lend_cpu(pid_t pid, int cpuid, pid_t *new_guest) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner == pid) {
        // If the CPU is owned by the process, just change the state
        cpuinfo->state = CPU_LENT;
    } else if (shdata->queues_enabled) {
        // Otherwise, remove any previous request
        queue_pids_remove(&cpuinfo->requests, pid);
    }

    // If the process is the guest, free it
    if (cpuinfo->guest == pid) {
        cpuinfo->guest = NOBODY;
        update_cpu_stats(cpuid, STATS_IDLE);
    }

    // If the CPU is free, find a new guest
    if (cpuinfo->guest == NOBODY) {
        *new_guest = find_new_guest(cpuinfo);
        if (*new_guest != NOBODY) {
            cpuinfo->guest = *new_guest;
        }
    } else {
        *new_guest = -1;
    }

    shdata->timestamp_cpu_lent = get_time_in_ns();
}

int shmem_cpuinfo__lend_cpu(pid_t pid, int cpuid, pid_t *new_guest) {
    int error = DLB_SUCCESS;
    //DLB_DEBUG( cpu_set_t freed_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    //DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        lend_cpu(pid, cpuid, new_guest);

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

    //DLB_DEBUG( int size = CPU_COUNT(&freed_cpus); )
    //DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    //verbose(VB_SHMEM, "Lending %s", mu_to_str(&freed_cpus));
    //verbose(VB_SHMEM, "Increasing %d Idle Threads (%d now)", size, post_size);
    //verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    //add_event(IDLE_CPUS_EVENT, idle_count);
    return error;
}

int shmem_cpuinfo__lend_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]) {
    int error = DLB_SUCCESS;

    //DLB_DEBUG( cpu_set_t freed_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    //DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid = 0; cpuid < node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                lend_cpu(pid, cpuid, &new_guests[cpuid]);

            //// Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            //if (is_idle(cpuid)) {
                //DLB_INSTR( idle_count++; )
                //DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            //}
            } else {
                new_guests[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);

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
static int reclaim_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
    if (cpuinfo->owner == pid) {
        cpuinfo->state = CPU_BUSY;
        if (cpuinfo->guest == pid) {
            *new_guest = -1;
            *victim = -1;
            error = DLB_NOUPDT;
        }
        else if (cpuinfo->guest == NOBODY) {
            cpuinfo->guest = pid;
            update_cpu_stats(cpuid, STATS_OWNED);
            *new_guest = pid;
            *victim = -1;
            error = DLB_SUCCESS;
        } else {
            *new_guest = pid;
            *victim = cpuinfo->guest;
            error = DLB_NOTED;
        }
    } else {
        *new_guest = -1;
        *victim = -1;
        error = DLB_ERR_PERM;
    }
    return error;
}

int shmem_cpuinfo__reclaim_all(pid_t pid, pid_t new_guests[], pid_t victims[]) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (shdata->node_info[cpuid].owner == pid
                    && shdata->node_info[cpuid].guest != pid) {
                int local_error = reclaim_cpu(pid, cpuid, &new_guests[cpuid], &victims[cpuid]);
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
            } else {
                new_guests[cpuid] = -1;
                victims[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__reclaim_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim) {
    int error;
    //DLB_DEBUG( cpu_set_t recovered_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&recovered_cpus); )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        error = reclaim_cpu(pid, cpuid, new_guest, victim);

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

int shmem_cpuinfo__reclaim_cpus(pid_t pid, int ncpus, pid_t new_guests[], pid_t victims[]) {
    int error = DLB_NOUPDT;
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    //cpu_set_t recovered_cpus;
    //CPU_ZERO(&recovered_cpus);

    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size && ncpus>0; ++cpuid) {
            int local_error = reclaim_cpu(pid, cpuid, &new_guests[cpuid], &victims[cpuid]);
            switch(local_error) {
                case DLB_NOTED:
                    // max priority, always overwrite
                    error = DLB_NOTED;
                    --ncpus;
                    break;
                case DLB_SUCCESS:
                    // medium priority, only update if error is in lowest priority
                    error = (error == DLB_NOTED) ? DLB_NOTED : DLB_SUCCESS;
                    --ncpus;
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

        /* Invalidate arguments out of ncpus */
        for (; cpuid<node_size; ++cpuid) {
            new_guests[cpuid] = -1;
            victims[cpuid] = -1;
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

int shmem_cpuinfo__reclaim_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[],
        pid_t victims[]) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                int local_error = reclaim_cpu(pid, cpuid, &new_guests[cpuid],
                        &victims[cpuid]);
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
            } else {
                new_guests[cpuid] = -1;
                victims[cpuid] = -1;
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
static int acquire_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->guest == pid) {
        // CPU already guested
        *new_guest = -1;
        *victim = -1;
        error = DLB_NOUPDT;
    } else if (cpuinfo->owner == pid) {
        // CPU is owned by the process
        cpuinfo->state = CPU_BUSY;
        if (cpuinfo->guest == NOBODY) {
            cpuinfo->guest = pid;
            *new_guest = pid;
            *victim = -1;
            error = DLB_SUCCESS;
            update_cpu_stats(cpuid, STATS_OWNED);
        } else {
            *new_guest = pid;
            *victim = cpuinfo->guest;
            error = DLB_NOTED;
        }
    } else if (cpuinfo->state == CPU_LENT && cpuinfo->guest == NOBODY) {
        // CPU is available
        cpuinfo->guest = pid;
        *new_guest = pid;
        *victim = -1;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_GUESTED);
    } else if (cpuinfo->state != CPU_DISABLED) {
        // CPU is busy, or lent to another process
        *new_guest = -1;
        *victim = -1;
        if (shdata->queues_enabled) {
            error = queue_pids_push(&cpuinfo->requests, pid);
        } else {
            error = DLB_NOUPDT;
        }
    } else {
        // CPU is disabled
        *new_guest = -1;
        *victim = -1;
        error = DLB_ERR_PERM;
    }

    return error;
}

int shmem_cpuinfo__acquire_cpu(pid_t pid, int cpuid, pid_t *new_guest, pid_t *victim) {
    int error;
    shmem_lock(shm_handler);
    {
        error = acquire_cpu(pid, cpuid, new_guest, victim);
    }
    shmem_unlock(shm_handler);
    return error;
}

/* exceptional case: acquire_cpus may need borrow_cpu  */
static int borrow_cpu(pid_t pid, int cpuid, pid_t *victim);

int shmem_cpuinfo__acquire_ncpus_from_cpu_subset(pid_t pid, int *requested_ncpus,
        int cpus_priority_array[node_size], priority_t priority, int max_parallelism,
        int64_t *last_borrow, pid_t new_guests[node_size], pid_t victims[node_size]) {

    int i;

    /* Return immediately if requested_ncpus is present and not greater than zero */
    if (requested_ncpus && *requested_ncpus <= 0) {
        return DLB_NOUPDT;
    }

    /* Return immediately if there is nothing left to acquire */
    /* 1) If the timestamp of the last unsuccessful borrow is newer than the last CPU lent */
    if (last_borrow && *last_borrow > shdata->timestamp_cpu_lent) {
        /* 2) Unless there's an owned CPUs not guested, in that case we will acquire anyway */
        bool all_owned_cpus_are_guested = true;
        for (i=0; i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            /* Iterate until the first invalid cpuid */
            if (cpuid == -1) break;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
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
        for (i=0; i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            if (cpuid == -1) break;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
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

    /* Functions that iterate cpus_priority_array may not check every CPU,
     * output arrays need to be properly initialized
     */
    initialize_output_array(new_guests);
    initialize_output_array(victims);

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        /* Note: cpus_priority_array always have owned CPUs first so we split the
        *  algorithm in loops with different body for owned and non-owned CPUs
        */

        /* Acquire first owned CPUs that are IDLE */
        for (i=0; ncpus>0 && i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            /* Break if cpu array does not contain more valid CPU ids */
            if (cpuid == -1) break;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            /* Go to next loop if cpu array does not contain owned CPUs */
            if (cpuinfo->owner != pid) break;
            /* Skip CPU if not IDLE */
            if (cpuinfo->guest != NOBODY) continue;

            int local_error = acquire_cpu(pid, cpuid,
                    &new_guests[cpuid], &victims[cpuid]);
            if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
                /* Update ncpus */
                --ncpus;
            }
        }

        /* Acquire owned CPUs following the priority of cpus_priority_array */
        for (i=0; ncpus>0 && i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            /* Break if cpu array does not contain more valid CPU ids */
            if (cpuid == -1) break;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            /* Go to next loop if cpu array does not contain owned CPUs */
            if (cpuinfo->owner != pid) break;
            /* Skip CPU if already guested */
            if (cpuinfo->guest == pid) continue;

            int local_error = acquire_cpu(pid, cpuid,
                    &new_guests[cpuid], &victims[cpuid]);
            if (local_error == DLB_SUCCESS || local_error == DLB_NOTED) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
                /* Update ncpus */
                --ncpus;
            }
        }

        /* Borrow non-owned CPUs following the priority of cpus_priority_array */
        for (; ncpus>0 && i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            /* Break if cpu array does not contain more valid CPU ids */
            if (cpuid == -1) break;

            int local_error = borrow_cpu(pid, cpuid, &new_guests[cpuid]);
            if (local_error == DLB_SUCCESS) {
                /* Update error code if needed */
                if (error != DLB_NOTED) error = local_error;
                /* Update ncpus */
                --ncpus;
            }
        }

        /* Add global petition for remaining CPUs if needed */
        if (shdata->queues_enabled
                && requested_ncpus
                && ncpus > 0) {
            /* Construct a mask of allowed CPUs */
            cpu_set_t allowed;
            CPU_ZERO(&allowed);
            for (i=0; i<node_size; ++i) {
                int cpuid = cpus_priority_array[i];
                if (cpuid != -1) {
                    CPU_SET(cpuid, &allowed);
                }
            }

            /* Enqueue request */
            verbose(VB_SHMEM, "Requesting %d CPUs more after acquiring", ncpus);
            error = queue_proc_reqs_push(&shdata->proc_requests, pid, ncpus, &allowed);
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

static int borrow_cpu(pid_t pid, int cpuid, pid_t *new_guest) {
    int error = DLB_NOUPDT;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner == pid && cpuinfo->guest == NOBODY) {
        // CPU is owned by the process
        cpuinfo->state = CPU_BUSY;
        cpuinfo->guest = pid;
        *new_guest = pid;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_OWNED);
    } else if (cpuinfo->state == CPU_LENT && cpuinfo->guest == NOBODY) {
        // CPU is available
        cpuinfo->guest = pid;
        *new_guest = pid;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_GUESTED);
    } else {
        *new_guest = -1;
    }

    return error;
}

int shmem_cpuinfo__borrow_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error;
    shmem_lock(shm_handler);
    {
        error = borrow_cpu(pid, cpuid, victim);
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__borrow_ncpus_from_cpu_subset(pid_t pid, int *requested_ncpus,
        int cpus_priority_array[node_size], priority_t priority, int max_parallelism,
        int64_t *last_borrow, pid_t new_guests[node_size]) {

    int i;

    /* Return immediately if requested_ncpus is present and not greater than zero */
    if (requested_ncpus && *requested_ncpus <= 0) {
        return DLB_NOUPDT;
    }

    /* Return immediately if the timestamp of the last unsuccessful borrow is newer than the last CPU lent */
    if (last_borrow && *last_borrow > shdata->timestamp_cpu_lent) {
        return DLB_NOUPDT;
    }

    /* Return immediately if the process has reached the max_parallelism */
    if (max_parallelism != 0) {
        for (i=0; i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            if (cpuid == -1) break;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
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

    /* Functions that iterate cpus_priority_array may not check every CPU,
     * the output array needs to be properly initialized
     */
    initialize_output_array(new_guests);

    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        /* Borrow CPUs following the priority of cpus_priority_array */
        for (i=0; ncpus>0 && i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            /* Break if cpu array does not contain more valid CPU ids */
            if (cpuid == -1) break;

            if (borrow_cpu(pid, cpuid, &new_guests[cpuid]) == DLB_SUCCESS) {
                --ncpus;
                error = DLB_SUCCESS;
            }
        }

        /* Only in case --priority=spread-ifempty, the CPU candidates cannot
         * be precomputed since they depend on the current state of each CPU
         */
        if (priority == PRIO_SPREAD_IFEMPTY && ncpus > 0) {
            // Check also empty sockets
            cpu_set_t free_mask;
            CPU_ZERO(&free_mask);
            int cpuid;
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                if (shdata->node_info[cpuid].state == CPU_LENT
                        && shdata->node_info[cpuid].guest == NOBODY) {
                    CPU_SET(cpuid, &free_mask);
                }
            }
            cpu_set_t free_sockets;
            mu_get_parents_inside_cpuset(&free_sockets, &free_mask);
            for (cpuid=0; ncpus>0 && cpuid<node_size; ++cpuid) {
                if (CPU_ISSET(cpuid, &free_sockets)) {
                    if (borrow_cpu(pid, cpuid, &new_guests[cpuid]) == DLB_SUCCESS) {
                        error = DLB_SUCCESS;
                    }
                }
            }
        }

        /* Update timestamp if borrow did not succeed */
        if (last_borrow != NULL && error != DLB_SUCCESS) {
            *last_borrow = get_time_in_ns();
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*  Return CPU                                                                   */
/*********************************************************************************/

/* Return CPU
 * Abandon CPU given that state == BUSY, owner != pid, guest == pid
 */
static int return_cpu(pid_t pid, int cpuid, pid_t *new_guest) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    // Return CPU
    cpuinfo->guest = NOBODY;
    update_cpu_stats(cpuid, STATS_IDLE);

    // Find a new guest
    *new_guest = find_new_guest(cpuinfo);
    if (*new_guest != NOBODY) {
        cpuinfo->guest = *new_guest;
    }

    int error;
    if (shdata->queues_enabled) {
        // Add another CPU request
        error = queue_pids_push(&cpuinfo->requests, pid);
        error = error < 0 ? error : DLB_SUCCESS;
    } else {
        error = DLB_SUCCESS;
    }

    return error;
}

int shmem_cpuinfo__return_all(pid_t pid, pid_t new_guests[]) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->state == CPU_BUSY
                    && cpuinfo->owner != pid
                    && cpuinfo->guest == pid) {
                int local_error = return_cpu(pid, cpuid, &new_guests[cpuid]);
                error = (error < 0) ? error : local_error;
            } else {
                new_guests[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, pid_t *new_guest) {
    int error;
    shmem_lock(shm_handler);
    {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (cpuinfo->owner == pid || cpuinfo->state == CPU_LENT) {
            error = DLB_NOUPDT;
        } else if (cpuinfo->guest != pid) {
            error = DLB_ERR_PERM;
        } else {
            error = return_cpu(pid, cpuid, new_guest);
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t new_guests[]) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (CPU_ISSET(cpuid, mask)
                    && cpuinfo->state == CPU_BUSY
                    && cpuinfo->owner != pid
                    && cpuinfo->guest == pid) {
                int local_error = return_cpu(pid, cpuid, &new_guests[cpuid]);
                error = (error < 0) ? error : local_error;
            } else {
                new_guests[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/

/* Called when lewi_mask_finalize.
 * This function deregisters pid, disabling or lending CPUs as needed */
int shmem_cpuinfo__deregister(pid_t pid, pid_t new_guests[], pid_t victims[]) {
    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        int cpuid;

        // Remove any request before acquiring and lending
        if (shdata->queues_enabled) {
            queue_proc_reqs_remove(&shdata->proc_requests, pid);
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                if (cpuinfo->owner != pid) {
                    queue_pids_remove(&cpuinfo->requests, pid);
                }
            }
        }

        // Iterate again to properly treat each CPU
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                if (cpu_is_public_post_mortem) {
                    /* If the CPU is being guested, lend_cpu does not reclaim it */
                    lend_cpu(pid, cpuid, &new_guests[cpuid]);
                    victims[cpuid] = -1;
                } else {
                    /* If CPU won't be public, it must be reclaimed beforehand */
                    reclaim_cpu(pid, cpuid, &new_guests[cpuid], &victims[cpuid]);
                    new_guests[cpuid] = -1;
                    if (cpuinfo->guest == pid) {
                        cpuinfo->guest = NOBODY;
                        update_cpu_stats(cpuid, STATS_IDLE);
                    }
                    cpuinfo->state = CPU_DISABLED;
                }
                cpuinfo->owner = NOBODY;
            } else {
                // Free external CPUs that I may be using
                if (cpuinfo->guest == pid) {
                    lend_cpu(pid, cpuid, &new_guests[cpuid]);
                } else {
                    new_guests[cpuid] = -1;
                }
                victims[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

/* Called when DLB_disable.
 * This function resets the initial status of pid: acquire owned, lend guested */
int shmem_cpuinfo__reset(pid_t pid, pid_t new_guests[], pid_t victims[]) {
    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        int cpuid;

        // Remove any request before acquiring and lending
        if (shdata->queues_enabled) {
            queue_proc_reqs_remove(&shdata->proc_requests, pid);
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                if (cpuinfo->owner != pid) {
                    queue_pids_remove(&cpuinfo->requests, pid);
                }
            }
        }

        // Iterate again to properly reset each CPU
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                reclaim_cpu(pid, cpuid, &new_guests[cpuid], &victims[cpuid]);
            } else if (cpuinfo->guest == pid) {
                lend_cpu(pid, cpuid, &new_guests[cpuid]);
                victims[cpuid] = pid;
            } else {
                new_guests[cpuid] = -1;
                victims[cpuid] = -1;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

/* Lend as many CPUs as needed to only guest as much as 'max' CPUs */
int shmem_cpuinfo__update_max_parallelism(pid_t pid, int max,
        pid_t new_guests[], pid_t victims[]) {
    int error = DLB_SUCCESS;
    int owned_count = 0;
    int guested_cpus[node_size];
    int guested_count = 0;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            new_guests[cpuid] = -1;
            victims[cpuid] = -1;
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                ++owned_count;
                if (max < owned_count) {
                    // Lend owned CPUs if the number of owned is greater than max
                    lend_cpu(pid, cpuid, &new_guests[cpuid]);
                    victims[cpuid] = pid;
                }
            } else if (cpuinfo->guest == pid) {
                // Since owned_count is still unkown, just save our guested CPUs
                guested_cpus[guested_count++] = cpuid;
            }
        }

        // Iterate guested CPUs to lend them if needed
        int i;
        for (i=0; i<guested_count; ++i) {
            if (max < owned_count + i + 1) {
                cpuid = guested_cpus[i];
                lend_cpu(pid, cpuid, &new_guests[cpuid]);
                victims[cpuid] = pid;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

/* Update CPU ownership according to the new process mask.
 * To avoid collisions, we only release the ownership if we still own it
 */
void shmem_cpuinfo__update_ownership(pid_t pid, const cpu_set_t *process_mask) {
    shmem_lock(shm_handler);
    verbose(VB_SHMEM, "Updating ownership: %s", mu_to_str(process_mask));
    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (CPU_ISSET(cpuid, process_mask)) {
            // The CPU should be mine
            if (cpuinfo->owner != pid) {
                // Not owned: Steal CPU
                cpuinfo->owner = pid;
                if (cpuinfo->guest == NOBODY) {
                    cpuinfo->guest = pid;
                }
                cpuinfo->state = CPU_BUSY;
                update_cpu_stats(cpuid, STATS_OWNED);

                verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpuid);
            } else {
                // The CPU was already owned, no update needed
            }

        } else {
            // The CPU is now not mine
            if (cpuinfo->owner == pid) {
                // Previusly owned: Release CPU ownership
                cpuinfo->owner = NOBODY;
                cpuinfo->state = CPU_DISABLED;
                if (cpuinfo->guest == pid ) {
                    cpuinfo->guest = NOBODY;
                }
                update_cpu_stats(cpuid, STATS_IDLE);
                verbose(VB_SHMEM, "Releasing ownership of CPU %d", cpuid);
            } else {
                // The CPU was not mine, no update needed
            }
        }
    }

    shmem_unlock(shm_handler);
}

int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num) {
    if (unlikely(shm_handler == NULL || thread_num < 0)) return -1;

    int owned_count = 0;
    int guested_cpus[node_size];
    int guested_count = 0;

    int cpuid;
    for (cpuid=0; cpuid<node_size; ++cpuid) {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
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
                update_cpu_stats(cpuid, STATS_OWNED);
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

bool shmem_cpuinfo__exists(void) {
    return shm_handler != NULL;
}

void shmem_cpuinfo__enable_request_queues(void) {
    if (shm_handler == NULL) return;

    /* Enable asynchronous request queues */
    shdata->queues_enabled = true;
}

void shmem_cpuinfo__remove_requests(pid_t pid) {
    if (shm_handler == NULL) return;
    shmem_lock(shm_handler);
    {
        /* Remove any previous request for the specific pid */
        if (shdata->queues_enabled) {
            queue_proc_reqs_remove(&shdata->proc_requests, pid);
        }
    }
    shmem_unlock(shm_handler);
}

void shmem_cpuinfo__print_cpu_times(void){

    if (shm_handler == NULL) return;

    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if( shdata->node_info[cpuid].acc_time[0] == 0 && shdata->node_info[cpuid].acc_time[1] == 0 && shdata->node_info[cpuid].acc_time[2]==0)continue;
            double double_idle =     (double) (shdata->node_info[cpuid].acc_time[0]) *1.0e-9;
            double double_owned =    (double) (shdata->node_info[cpuid].acc_time[1]) *1.0e-9;
            double double_guested  = (double) (shdata->node_info[cpuid].acc_time[2]) *1.0e-9;
            double sum = double_idle + double_owned + double_guested;
            info("      STATS_IDLE     : %lf%%\n", (double_idle / sum) * 100);
            info("      STATS_OWNED    : %lf%%\n", (double_owned / sum )* 100 );
            info("      STATS_GUESTED     : %lf%%\n\n", (double_guested / sum)* 100 );
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

/* External Functions
 * These functions are intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize the shared memory
 */

int shmem_cpuinfo_ext__getnumcpus(void) {
    return mu_get_system_size();
}

float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state) {
    if (shm_handler == NULL) {
        return -1.0f;
    }

    float usage;
    shmem_lock(shm_handler);
    {
        usage = getcpustate(cpu, state, shdata);
    }
    shmem_unlock(shm_handler);

    return usage;
}

void shmem_cpuinfo__print_info(const char *shmem_key, int columns,
        dlb_printshmem_flags_t print_flags) {

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_cpuinfo_ext__init(shmem_key);
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
            width = w.ws_col;
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
                cpuinfo_t *cpuinfo = &shdata_copy->node_info[cpuid];
                pid_t owner = cpuinfo->owner;
                pid_t guest = cpuinfo->guest;
                cpu_state_t state = cpuinfo->state;
                if (color) {
                    const char *code_color =
                        state == CPU_DISABLED               ? ANSI_COLOR_RESET :
                        state == CPU_BUSY && guest == owner ? ANSI_COLOR_RED :
                        state == CPU_BUSY                   ? ANSI_COLOR_YELLOW :
                        guest == NOBODY                     ? ANSI_COLOR_GREEN :
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

    /* Cpu requests */
    bool any_cpu_request = false;
    for (cpuid=0; cpuid<node_size && !any_cpu_request; ++cpuid) {
        any_cpu_request = queue_pids_size(&shdata_copy->node_info[cpuid].requests) > 0;
    }
    if (any_cpu_request) {
        snprintf(line, MAX_LINE_LEN, "\n  Cpu requests (<cpuid>: <spids>):");
        printbuffer_append(&buffer, line);
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            queue_pids_t *requests = &shdata_copy->node_info[cpuid].requests;
            if (queue_pids_size(requests) > 0) {
                /* Set up line */
                line[0] = '\0';
                l = line;
                l += snprintf(l, MAX_LINE_LEN-strlen(line), "  %4d: ", cpuid);
                /* Iterate requests */
                while (queue_pids_size(requests) > 0) {
                    pid_t pid;
                    queue_pids_pop(requests, &pid);
                    l += snprintf(l, MAX_LINE_LEN-strlen(line), " %u,", pid);
                }
                /* Remove trailing comma and append line */
                *(l-1) = '\0';
                printbuffer_append(&buffer, line);
            }
        }
    }

    /* Proc requests */
    if (queue_proc_reqs_size(&shdata_copy->proc_requests) > 0) {
        snprintf(line, MAX_LINE_LEN,
                "\n  Process requests (<spids>: <howmany>, <allowed_cpus>):");
        printbuffer_append(&buffer, line);
    }
    while (queue_proc_reqs_size(&shdata_copy->proc_requests) > 0) {
        process_request_t *request = queue_proc_reqs_front(&shdata_copy->proc_requests);
        snprintf(line, MAX_LINE_LEN,
                "    %*d: %d, %s",
                max_digits, request->pid, request->howmany,
                mu_to_str(&request->allowed));
        queue_proc_reqs_pop(&shdata_copy->proc_requests, NULL);
        printbuffer_append(&buffer, line);
    }

    info0("=== CPU States ===\n%s", buffer.addr);
    printbuffer_destroy(&buffer);
    free(shdata_copy);
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

static void update_cpu_stats(int cpu, stats_state_t new_state) {
    // skip update if old_state == new_state
    if (shdata->node_info[cpu].stats_state == new_state) return;

    struct timespec now;
    int64_t elapsed;

    // Compute elapsed since last update
    get_time(&now);
    elapsed = timespec_diff(&shdata->node_info[cpu].last_update, &now);

    // Update fields
    stats_state_t old_state = shdata->node_info[cpu].stats_state;
    shdata->node_info[cpu].acc_time[old_state] += elapsed;
    shdata->node_info[cpu].stats_state = new_state;
    shdata->node_info[cpu].last_update = now;
}

static float getcpustate(int cpu, stats_state_t state, shdata_t *shared_data) {
    struct timespec now;
    get_time_coarse(&now);
    int64_t total = timespec_diff(&shared_data->initial_time, &now);
    int64_t acc = shared_data->node_info[cpu].acc_time[state];
    float percentage = (float)acc/total;
    ensure(percentage >= -0.000001f && percentage <= 1.000001f,
                "Percentage out of bounds");
    return percentage;
}
/*** End of helper functions ***/
