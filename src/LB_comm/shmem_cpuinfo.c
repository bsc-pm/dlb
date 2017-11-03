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

#include "LB_comm/shmem_cpuinfo.h"

#include "LB_comm/shmem.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

/* NOTE on default values:
 * The shared memory will be initializated to 0 when created,
 * thus all default values should represent 0
 * A CPU, by default, is DISABLED and owned by NOBODY
 */

enum { NOBODY = 0 };


/*** Global request queue: Processes make requests for N non-specific CPUs ***/
enum { GLOBAL_QUEUE_SIZE = 100 };
typedef struct {
    pid_t        pid;
    unsigned int howmany;
} process_request_t;
typedef struct {
    process_request_t queue[GLOBAL_QUEUE_SIZE];
    unsigned int      head;
    unsigned int      tail;
} global_request_t;

static void remove_global_request(global_request_t *queue, pid_t pid) {
    unsigned int i;
    for (i=queue->tail; i!=queue->head; i=(i+1)%GLOBAL_QUEUE_SIZE) {
        if (queue->queue[i].pid == pid) {
            queue->queue[i].pid = NOBODY;
            queue->queue[i].howmany = 0;
        }
    }
}

static int push_global_request(global_request_t *queue, pid_t applicant, unsigned int howmany) {
    if (howmany == 0) {
        remove_global_request(queue, applicant);
        return DLB_SUCCESS;
    }
    unsigned int next_head = (queue->head + 1) % GLOBAL_QUEUE_SIZE;
    if (__builtin_expect((next_head == queue->tail), 0)) {
        return DLB_ERR_REQST;
    }
    queue->queue[queue->head].pid = applicant;
    queue->queue[queue->head].howmany = howmany;
    queue->head = next_head;
    return DLB_NOTED;
}

static void pop_global_request(global_request_t *queue, pid_t *new_guest) {
    *new_guest = NOBODY;
    while(queue->head != queue->tail && *new_guest == NOBODY) {
        process_request_t *request = &queue->queue[queue->tail];
        if (request->pid != NOBODY && request->howmany > 0) {
            /* request is valid */
            *new_guest = request->pid;
            if (request->howmany-- == 0) {
                queue->tail = (queue->tail + 1) % GLOBAL_QUEUE_SIZE;
            }
        } else {
            /* request is not valid, advance tail */
            queue->tail = (queue->tail + 1) % GLOBAL_QUEUE_SIZE;
        }
    }
}
/*****************************************************************************/

/****** CPU request queue: Processes make request for this specific CPU ******/
enum { CPU_QUEUE_SIZE = 8 };
typedef struct {
    pid_t        queue[CPU_QUEUE_SIZE];
    unsigned int head;
    unsigned int tail;
} cpu_request_t;

static void remove_cpu_request(cpu_request_t *queue, pid_t pid) {
    unsigned int i;
    for (i=queue->tail; i!=queue->head; i=(i+1)%CPU_QUEUE_SIZE) {
        if (queue->queue[i] == pid) {
            queue->queue[i] = NOBODY;
        }
    }
}

static int push_cpu_request(cpu_request_t *queue, pid_t applicant) {
    unsigned int next_head = (queue->head + 1) % CPU_QUEUE_SIZE;
    if (__builtin_expect((next_head == queue->tail), 0)) {
        return DLB_ERR_REQST;
    }
    queue->queue[queue->head] = applicant;
    queue->head = next_head;
    return DLB_NOTED;
}

static void pop_cpu_request(cpu_request_t *queue, pid_t *new_guest) {
    *new_guest = NOBODY;
    while(queue->head != queue->tail && *new_guest == NOBODY) {
        *new_guest = queue->queue[queue->tail];
        queue->tail = (queue->tail + 1) % CPU_QUEUE_SIZE;
    }
}
/*****************************************************************************/


typedef enum {
    CPU_DISABLED = 0,       // Not owned by any process nor part of the DLB mask
    CPU_BUSY,
    CPU_LENT
} cpu_state_t;

typedef struct {
    int             id;
    pid_t           owner;                  // Current owner, useful to resolve reclaimed CPUs
    pid_t           guest;                  // Current user of the CPU
    bool            dirty;
    cpu_state_t     state;
    stats_state_t   stats_state;
    int64_t         acc_time[_NUM_STATS];   // Accumulated time for each state
    struct          timespec last_update;
    cpu_request_t   requests;
} cpuinfo_t;

typedef struct {
    bool             dirty;
    struct           timespec initial_time;
    global_request_t global_requests;
    cpuinfo_t        node_info[0];
} shdata_t;

enum { SHMEM_CPUINFO_VERSION = 1 };

static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int node_size;
static bool cpu_is_public_post_mortem = false;
static const char *shmem_name = "cpuinfo";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static inline bool is_idle(int cpu);
static inline bool is_borrowed(pid_t pid, int cpu);
static void update_cpu_stats(int cpu, stats_state_t new_state);
static float getcpustate(int cpu, stats_state_t state, shdata_t *shared_data);


static pid_t find_new_guest(cpuinfo_t *cpuinfo) {
    pid_t new_guest;
    if (cpuinfo->state == CPU_BUSY) {
        /* If CPU is claimed, ignore requests an assign owner */
        new_guest = cpuinfo->owner;
    } else {
        /* Pop first in CPU queue */
        pop_cpu_request(&cpuinfo->requests, &new_guest);

        /* If CPU did noy have requests, pop global queue */
        if (new_guest == NOBODY) {
            pop_global_request(&shdata->global_requests, &new_guest);
        }
    }
    return new_guest;
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

static int register_process(pid_t pid, const cpu_set_t *mask, bool steal) {
    int cpuid;
    if (!steal) {
        // Check first that my mask is not already owned
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)
                    && shdata->node_info[cpuid].owner != NOBODY
                    && shdata->node_info[cpuid].owner != pid) {
                return DLB_ERR_PERM;
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
            cpuinfo->id = cpuid;
            cpuinfo->owner = pid;
            cpuinfo->state = CPU_BUSY;
            cpuinfo->dirty = false;
            update_cpu_stats(cpuid, STATS_OWNED);
        }
    }

    return DLB_SUCCESS;
}

int shmem_cpuinfo__init(pid_t pid, const cpu_set_t *process_mask, const char *shmem_key) {
    int error = DLB_SUCCESS;

    // Shared memory creation
    open_shmem(shmem_key);

    //cpu_set_t affinity_mask;
    //mu_get_parents_covering_cpuset(&affinity_mask, process_mask);

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (shdata->initial_time.tv_sec == 0 && shdata->initial_time.tv_nsec == 0) {
            get_time(&shdata->initial_time);
            shdata->dirty = false;
        }

        // Register process_mask, with stealing = false always in normal Init()
        error = register_process(pid, process_mask, false);
    }
    shmem_unlock(shm_handler);

    // TODO mask info should go in shmem_procinfo. Print something else here?
    //verbose( VB_SHMEM, "Process Mask: %s", mu_to_str(process_mask) );
    //verbose( VB_SHMEM, "Process Affinity Mask: %s", mu_to_str(&affinity_mask) );

    //add_event(IDLE_CPUS_EVENT, idle_count);

    return error;
}

int shmem_cpuinfo_ext__init(const char *shmem_key) {
    open_shmem(shmem_key);
    return DLB_SUCCESS;
}

int shmem_cpuinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, int steal) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;
    int error;
    shmem_lock(shm_handler);
    {
        // Initialize initial time and CPU timing on shmem creation
        if (shdata->initial_time.tv_sec == 0 && shdata->initial_time.tv_nsec == 0) {
            get_time(&shdata->initial_time);
        }

        error = register_process(pid, mask, steal);
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

static bool deregister_process(pid_t pid) {
    bool shmem_empty = true;
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
            } else {
                cpuinfo->state = CPU_DISABLED;
            }
        } else {
            // Free external CPUs that I may be using
            if (cpuinfo->guest == pid) {
                cpuinfo->guest = NOBODY;
                update_cpu_stats(cpuid, STATS_IDLE);
            }

            // Check if shmem is empty
            if (cpuinfo->owner != NOBODY) {
                shmem_empty = false;
            }
        }
    }
    return shmem_empty;
}

int shmem_cpuinfo__finalize(pid_t pid) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    //DLB_INSTR( int idle_count = 0; )

    // Boolean for now, we could make deregister_process return an integer and instrument it
    bool shmem_empty;

    // Lock the shmem to deregister CPUs
    shmem_lock(shm_handler);
    {
        shmem_empty = deregister_process(pid);
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
        // We just call deregister_process to check if shmem is empty
        shmem_empty = deregister_process(-1);
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
/*  Add CPU                                                                      */
/*********************************************************************************/

/* Add cpu_mask to the Shared Mask
 * If the process originally owns the CPU:      State => CPU_LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
static void add_cpu(pid_t pid, int cpuid, pid_t *new_pid) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner == pid) {
        // If the CPU is owned by the process, just change the state
        cpuinfo->state = CPU_LENT;
    } else {
        // Otherwise, remove any previous request
        remove_cpu_request(&cpuinfo->requests, pid);
    }

    // If the process is the guest, free it
    if (cpuinfo->guest == pid) {
        cpuinfo->guest = NOBODY;
        update_cpu_stats(cpuid, STATS_IDLE);
    }

    // If a new_pid is requested, find a new guest
    if (new_pid && cpuinfo->guest == NOBODY) {
        pid_t new_guest = find_new_guest(cpuinfo);
        if (new_guest != NOBODY) {
            *new_pid = new_guest;
            cpuinfo->guest = new_guest;
        }
    }
}

int shmem_cpuinfo__add_cpu(pid_t pid, int cpuid, pid_t *new_pid) {
    int error = DLB_SUCCESS;
    //DLB_DEBUG( cpu_set_t freed_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    //DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        add_cpu(pid, cpuid, new_pid);

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

int shmem_cpuinfo__add_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *new_pids) {
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
                pid_t *new_pid = (new_pids) ? &new_pids[cpuid] : NULL;
                add_cpu(pid, cpuid, new_pid);

            //// Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            //if (is_idle(cpuid)) {
                //DLB_INSTR( idle_count++; )
                //DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            //}
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
/*  Recover CPU                                                                  */
/*********************************************************************************/

/* Recover CPU from the Shared Mask
 * CPUs that owner == ME:           State => CPU_BUSY
 * CPUs that guest == NOBODY        Guest => ME
 */
static int recover_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
    cpuinfo->state = CPU_BUSY;
    if (cpuinfo->guest == pid) {
        error = DLB_NOUPDT;
    }
    else if (cpuinfo->guest == NOBODY) {
        cpuinfo->guest = pid;
        update_cpu_stats(cpuid, STATS_OWNED);
        if (victim) *victim = pid;
        error = DLB_SUCCESS;
    } else {
        if (victim) *victim = cpuinfo->guest;
        error = DLB_NOTED;
    }
    return error;
}

int shmem_cpuinfo__recover_all(pid_t pid, pid_t *victimlist) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (shdata->node_info[cpuid].owner == pid) {
                // victimlist is mandatory in recover_all
                int local_error = recover_cpu(pid, cpuid, &victimlist[cpuid]);
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
                }
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__recover_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error;
    //DLB_DEBUG( cpu_set_t recovered_cpus; )
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&recovered_cpus); )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        if (shdata->node_info[cpuid].owner == pid) {
            error = recover_cpu(pid, cpuid, victim);
            // if (!error) //DLB_DEBUG( CPU_SET(cpu, &recovered_cpus); )
        } else {
            error = DLB_ERR_PERM;
        }

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

int shmem_cpuinfo__recover_cpus(pid_t pid, int ncpus, pid_t *victimlist) {
    int error = DLB_SUCCESS;
    //DLB_DEBUG( cpu_set_t idle_cpus; )
    //DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    //DLB_INSTR( int idle_count = 0; )

    //cpu_set_t recovered_cpus;
    //CPU_ZERO(&recovered_cpus);
    //max_resources = (max_resources) ? max_resources : INT_MAX;

    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size && ncpus>0; ++cpuid) {
            if (shdata->node_info[cpuid].owner == pid) {
                // victimlist is mandatory in recover_cpus
                recover_cpu(pid, cpuid, &victimlist[cpuid]);
                --ncpus;
                // if (!error) //CPU_SET(cpuid, &recovered_cpus);
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

int shmem_cpuinfo__recover_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *victimlist) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                if (shdata->node_info[cpuid].owner == pid) {
                    pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                    int local_error = recover_cpu(pid, cpuid, victim);
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
                    }
                } else {
                    error = DLB_ERR_PERM;
                    break;
                }
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
static int acquire_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->guest == pid) {
        // CPU already guested
        error = DLB_NOUPDT;
    } else if (cpuinfo->owner == pid) {
        // CPU is owned by the process
        cpuinfo->state = CPU_BUSY;
        if (cpuinfo->guest == NOBODY) {
            cpuinfo->guest = pid;
            if (victim) *victim = pid;
            error = DLB_SUCCESS;
            update_cpu_stats(cpuid, STATS_OWNED);
        } else {
            if (victim) *victim = cpuinfo->guest;
            error = DLB_NOTED;
        }
    } else if (cpuinfo->state == CPU_LENT && cpuinfo->guest == NOBODY) {
        // CPU is available
        cpuinfo->guest = pid;
        if (victim) *victim = pid;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_GUESTED);
    } else if (cpuinfo->state != CPU_DISABLED) {
        // CPU is busy, or lent to another process
        error = push_cpu_request(&cpuinfo->requests, pid);
    } else {
        // CPU is disabled
        error = DLB_ERR_PERM;
    }

    return error;
}

int shmem_cpuinfo__acquire_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error;
    shmem_lock(shm_handler);
    {
        error = acquire_cpu(pid, cpuid, victim);
    }
    shmem_unlock(shm_handler);
    return error;
}

/* exceptional case: acquire_cpus may need borrow_cpu  */
static int borrow_cpu(pid_t pid, int cpuid, pid_t *victim);

int shmem_cpuinfo__acquire_cpus(pid_t pid, priority_t priority, int *cpus_priority_array,
        int ncpus, pid_t *victimlist) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        if (ncpus == 0) {
            /* AcquireCPUs(0) has a special meaning of removing any previous request */
            remove_global_request(&shdata->global_requests, pid);
            error = DLB_SUCCESS;
        } else {
            /* Note: cpus_priority_array always have owned CPUs first so we split the
             * algorithm in two loops with different body for owned and non-owned CPUs
             */

            /* Acquire owned CPUs following the priority of cpus_priority_array */
            int i;
            for (i=0; ncpus>0 && i<node_size; ++i) {
                int cpuid = cpus_priority_array[i];
                if (cpuid == -1) break;
                if(shdata->node_info[cpuid].owner != pid) break;
                pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                if (acquire_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                    --ncpus;
                }
            }

            /* Borrow non-owned CPUs following the priority of cpus_priority_array */
            for (;ncpus>0 && i<node_size; ++i) {
                int cpuid = cpus_priority_array[i];
                if (cpuid == -1) break;
                pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                    --ncpus;
                }
            }

            /* Add global petition for remaining CPUs if needed */
            if (ncpus > 0) {
                verbose(VB_SHMEM, "Requesting %d CPUs more after acquiring", ncpus);
                error = push_global_request(&shdata->global_requests, pid, ncpus);
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__acquire_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *victimlist) {
    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                if (shdata->node_info[cpuid].guest != pid) {
                    pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                    int local_error = acquire_cpu(pid, cpuid, victim);
                    error = (error < 0) ? error : local_error;
                }
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*  Borrow CPU                                                                   */
/*********************************************************************************/

static int borrow_cpu(pid_t pid, int cpuid, pid_t *victim) {
    int error = DLB_NOUPDT;
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    if (cpuinfo->owner == pid && cpuinfo->guest == NOBODY) {
        // CPU is owned by the process
        cpuinfo->state = CPU_BUSY;
        cpuinfo->guest = pid;
        if (victim) *victim = pid;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_OWNED);
    } else if (cpuinfo->state == CPU_LENT && cpuinfo->guest == NOBODY) {
        // CPU is available
        cpuinfo->guest = pid;
        if (victim) *victim = pid;
        error = DLB_SUCCESS;
        update_cpu_stats(cpuid, STATS_GUESTED);
    }

    return error;
}

int shmem_cpuinfo__borrow_all(pid_t pid, priority_t priority, int *cpus_priority_array,
        pid_t *victimlist) {

    bool one_sucess = false;
    shmem_lock(shm_handler);
    {
        int i;
        for (i=0; i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            if (cpuid == -1) break;
            pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
            if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                one_sucess = true;
            }
        }

        if (priority == PRIO_AFFINITY_FULL) {
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
            for (cpuid=0; cpuid<node_size; ++cpuid) {
                if (CPU_ISSET(cpuid, &free_sockets)) {
                    pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                    if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                        one_sucess = true;
                    }
                }
            }
        }
    }
    shmem_unlock(shm_handler);

    return one_sucess ? DLB_SUCCESS : DLB_NOUPDT;
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

int shmem_cpuinfo__borrow_cpus(pid_t pid, priority_t priority, int *cpus_priority_array,
        int ncpus, pid_t *victimlist) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        /* Borrow CPUs following the priority of cpus_priority_array, */
        int i;
        for (i=0; ncpus>0 && i<node_size; ++i) {
            int cpuid = cpus_priority_array[i];
            if (cpuid == -1) break;
            pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
            if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                --ncpus;
            }
        }

        /* Only in case --priority=affinity_full, the CPU candidates cannot
         * be precomputed since it depends on the current state of each CPU
         */
        if (priority == PRIO_AFFINITY_FULL && ncpus > 0) {
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
                    pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                    if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                        --ncpus;
                    }
                }
            }
        }
    }
    shmem_unlock(shm_handler);

    return (ncpus == 0) ? DLB_SUCCESS : error;
}

int shmem_cpuinfo__borrow_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *victimlist) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                if (shdata->node_info[cpuid].guest != pid) {
                    pid_t *victim = (victimlist) ? &victimlist[cpuid] : NULL;
                    if (borrow_cpu(pid, cpuid, victim) == DLB_SUCCESS) {
                        error = DLB_SUCCESS;
                    }
                }
            }
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
static int return_cpu(pid_t pid, int cpuid, pid_t *new_pid) {
    cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];

    // Return CPU
    cpuinfo->guest = NOBODY;
    update_cpu_stats(cpuid, STATS_IDLE);

    // If a new_pid is requested, find a new guest
    if (new_pid) {
        pid_t new_guest = find_new_guest(cpuinfo);
        if (new_guest != NOBODY) {
            *new_pid = new_guest;
            cpuinfo->guest = new_guest;
        }
    }

    // Add another CPU request
    int error = push_cpu_request(&cpuinfo->requests, pid);

    /* FIXME: return DLB_SUCCESS in order to not break API. An alternative
     * could be to return DLB_NOTED here (as current process is doing a
     * request) and DLB_SUCCESS if the CPU is not claimed anymore by the time
     * ReturnCpu is called
     */
    if (error == DLB_NOTED)
        error = DLB_SUCCESS;

    return error;
}

int shmem_cpuinfo__return_all(pid_t pid, pid_t *new_pids) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->state == CPU_BUSY
                    && cpuinfo->owner != pid
                    && cpuinfo->guest == pid) {
                pid_t *new_pid = (new_pids) ? &new_pids[cpuid] : NULL;
                int local_error = return_cpu(pid, cpuid, new_pid);
                error = (error < 0) ? error : local_error;
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu(pid_t pid, int cpuid, pid_t *new_pid) {
    int error;
    shmem_lock(shm_handler);
    {
        cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
        if (cpuinfo->owner == pid || cpuinfo->state == CPU_LENT) {
            error = DLB_NOUPDT;
        } else if (cpuinfo->guest != pid) {
            error = DLB_ERR_PERM;
        } else {
            error = return_cpu(pid, cpuid, new_pid);
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_cpuinfo__return_cpu_mask(pid_t pid, const cpu_set_t *mask, pid_t *new_pids) {
    int error = DLB_NOUPDT;
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (CPU_ISSET(cpuid, mask)) {
                cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
                if (cpuinfo->state == CPU_BUSY
                        && cpuinfo->owner != pid
                        && cpuinfo->guest == pid) {
                    pid_t *new_pid = (new_pids) ? &new_pids[cpuid] : NULL;
                    int local_error = return_cpu(pid, cpuid, new_pid);
                    error = (error < 0) ? error : local_error;
                }
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/

void shmem_cpuinfo__reset(pid_t pid) {
    /* int error = DLB_NOUPDT; */
    shmem_lock(shm_handler);
    {
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            cpuinfo_t *cpuinfo = &shdata->node_info[cpuid];
            if (cpuinfo->owner == pid) {
                acquire_cpu(pid, cpuid, NULL);
            } else {
                add_cpu(pid, cpuid, NULL);
            }
        }
    }
    shmem_unlock(shm_handler);
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
                // Steal CPU
                cpuinfo->owner = pid;
                verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpuid);
            }
            if (cpuinfo->guest == NOBODY) {
                cpuinfo->guest = pid;
            }
            cpuinfo->state = CPU_BUSY;
            update_cpu_stats(cpuid, STATS_OWNED);

            // Dirty flags
            cpuinfo->dirty = true;
            shdata->dirty = true;

        } else {
            // The CPU is now not mine
            if (cpuinfo->owner == pid) {
                // Release CPU ownership
                cpuinfo->owner = NOBODY;
                cpuinfo->state = CPU_DISABLED;
                if (cpuinfo->guest == pid ) {
                    cpuinfo->guest = NOBODY;
                }
                update_cpu_stats(cpuid, STATS_IDLE);
                verbose(VB_SHMEM, "Releasing ownership of CPU %d", cpuid);
            }
            if (cpuinfo->guest == pid) {
                // If I'm just the guest, return it as if claimed
                if (cpuinfo->owner == NOBODY) {
                    cpuinfo->guest = NOBODY;
                    update_cpu_stats(cpuid, STATS_IDLE);
                } else {
                    cpuinfo->guest = cpuinfo->owner;
                    update_cpu_stats(cpuid, STATS_OWNED);
                }
            }
        }
    }
    shmem_unlock(shm_handler);
}

int shmem_cpuinfo__get_thread_binding(pid_t pid, int thread_num) {
    if (shm_handler == NULL) return -1;

    int binding = -1;
    shmem_lock(shm_handler);
    {
        bool all_cpus_clean = true;
        int cpuid;
        for (cpuid=0; cpuid<node_size; ++cpuid) {
            if (shdata->node_info[cpuid].owner == pid) {
                if (thread_num == 0) {
                    /* CPU that belongs to provided pid and thread_num */
                    binding = cpuid;
                    shdata->node_info[cpuid].dirty = false;
                }
                --thread_num;
            }
            /* Keep iterating all shmem to check if some CPU is dirty */
            if (all_cpus_clean && shdata->node_info[cpuid].dirty) {
                all_cpus_clean = false;
            }

            /* Do not keep iterating if we already found the binding and some CPU dirty */
            if (binding != 1 && !all_cpus_clean) {
                break;
            }
        }
        /* Clean shmem dirty flag if this was the last queried CPU */
        if (all_cpus_clean) {
            shdata->dirty = false;
        }
    }
    shmem_unlock(shm_handler);

    return binding;
}

bool shmem_cpuinfo__is_cpu_available(pid_t pid, int cpuid) {
    // If the CPU is free, assign it to myself
    if (shdata->node_info[cpuid].guest == NOBODY) {
        shmem_lock(shm_handler);
        if (shdata->node_info[cpuid].guest == NOBODY) {
            shdata->node_info[cpuid].guest = pid;
            update_cpu_stats(cpuid, STATS_OWNED);
        }
        shmem_unlock(shm_handler);
    }

    return shdata->node_info[cpuid].guest == pid;
}

bool shmem_cpuinfo__exists(void) {
    return shm_handler != NULL;
}

bool shmem_cpuinfo__is_dirty(void) {
    return shdata && shdata->dirty;
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

void shmem_cpuinfo_ext__print_info(bool statistics) {
    if (shm_handler == NULL) {
        warning("The shmem %s is not initialized, cannot print", shmem_name);
        return;
    }

    // Make a full copy of the shared memory. Basic size + zero-length array real length
    shdata_t *shdata_copy = malloc(sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size);

    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size);
    }
    shmem_unlock(shm_handler);

    char owners[512] = "OWNERS: ";
    char guests[512] = "GUESTS: ";
    char states[512] = "STATES: ";
    char *o = owners+8;
    char *g = guests+8;
    char *s = states+8;
    int cpu;
    for (cpu=0; cpu<node_size; cpu++) {
        o += snprintf(o, 8, "%d, ", shdata_copy->node_info[cpu].owner);
        g += snprintf(g, 8, "%d, ", shdata_copy->node_info[cpu].guest);
        s += snprintf(s, 8, "%d, ", shdata_copy->node_info[cpu].state);
    }

    info0("=== CPU States ===");
    info0(owners);
    info0(guests);
    info0(states);
    info0("States Legend: DISABLED=%d, BUSY=%d, LENT=%d", CPU_DISABLED, CPU_BUSY, CPU_LENT);

    if (statistics) {
        info0("=== CPU Statistics ===");
        for (cpu=0; cpu<node_size; ++cpu) {
            info0("CPU %d: OWNED(%.2f%%), GUESTED(%.2f%%), IDLE(%.2f%%)",
                    cpu,
                    getcpustate(cpu, STATS_OWNED, shdata_copy)*100,
                    getcpustate(cpu, STATS_GUESTED, shdata_copy)*100,
                    getcpustate(cpu, STATS_IDLE, shdata_copy)*100);
        }
    }

    free(shdata_copy);
}

/*** Helper functions, the shm lock must have been acquired beforehand ***/
static inline bool is_idle(int cpu) {
    return shdata->node_info[cpu].state == CPU_LENT && shdata->node_info[cpu].guest == NOBODY;
}

static inline bool is_borrowed(pid_t pid, int cpu) {
    return shdata->node_info[cpu].state == CPU_BUSY && shdata->node_info[cpu].owner == pid;
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
