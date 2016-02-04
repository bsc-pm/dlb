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
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

/* NOTE on default values:
 * The shared memory will be initializated to 0 when created,
 * thus all default values should represent 0
 * A CPU, by default, is DISABLED and owned by NOBODY
 */

#define NOBODY 0

typedef pid_t spid_t;  // Sub-process ID

typedef enum {
    CPU_DISABLED = 0,       // Not owned by any process nor part of the DLB mask
    CPU_BUSY,
    CPU_LENT
} cpu_state_t;

typedef struct {
    spid_t      owner;      // Current owner, useful to resolve reclaimed CPUs
    spid_t      guest;      // Current user of the CPU
    cpu_state_t state;
    stats_state_t stats_state;
    int64_t acc_time[_NUM_STATS];  // Accumulated time for each state
    struct timespec last_update;
} cpuinfo_t;

typedef struct {
    struct timespec initial_time;
    cpuinfo_t node_info[0];
} shdata_t;

static spid_t ME;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static cpu_set_t dlb_mask;       // CPUs not owned by any process but usable by others
static int node_size;
static bool cpu_is_public_post_mortem = false;

static inline bool is_idle(int cpu);
static void update_cpu_stats(int cpu, stats_state_t new_state);
static float getcpustate(int cpu, stats_state_t state);

void shmem_cpuinfo__init(void) {
    // Protect double initialization
    if (shm_handler != NULL) {
        warning("Shared Memory is being initialized more than once");
        print_backtrace();
        return;
    }

    ME = getpid();
    mu_parse_mask(options_get_mask(), &dlb_mask);
    cpu_set_t process_mask;
    get_process_mask(&process_mask);
    cpu_set_t affinity_mask;
    mu_get_affinity_mask(&affinity_mask, &process_mask, MU_ANY_BIT);
    node_size = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_handler_t *init_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size, "cpuinfo");

    DLB_INSTR( int idle_count = 0; )

    shmem_lock(init_handler);
    {
        int cpu;
        // Check first that my process_mask is not already owned
        for (cpu = 0; cpu < node_size; cpu++)
            if (CPU_ISSET(cpu, &process_mask) && shdata->node_info[cpu].owner != NOBODY) {
                spid_t owner = shdata->node_info[cpu].owner;
                shmem_unlock(init_handler);
                fatal("Error trying to acquire CPU %d, already owned by process %d", cpu, owner);
            }

        for (cpu = 0; cpu < node_size; cpu++) {
            // Add my mask info
            if (CPU_ISSET(cpu, &process_mask)) {
                shdata->node_info[cpu].owner = ME;
                shdata->node_info[cpu].state = CPU_BUSY;
                // My CPU could have already a guest if the DLB_mask is being used
                if (shdata->node_info[cpu].guest == NOBODY) {
                    shdata->node_info[cpu].guest = ME;
                    update_cpu_stats(cpu, STATS_OWNED);
                }
            }
            // Add DLB mask info
            if (CPU_ISSET(cpu, &dlb_mask)
                    && shdata->node_info[cpu].owner == NOBODY
                    && shdata->node_info[cpu].state == CPU_DISABLED) {
                shdata->node_info[cpu].state = CPU_LENT;
            }

            // Look for Idle CPUs
            DLB_INSTR( if (is_idle(cpu)) idle_count++; )
        }

        // Initialize initial time and CPU timing on shmem creation
        if (shdata->initial_time.tv_sec == 0 && shdata->initial_time.tv_nsec == 0) {
            get_time(&shdata->initial_time);
            for (cpu = 0; cpu < node_size; cpu++) {
                shdata->node_info[cpu].last_update = shdata->initial_time;
            }
        }
    }
    shmem_unlock(init_handler);

    // Global variable is only assigned after the initialization
    shm_handler = init_handler;

    // TODO mask info should go in shmem_procinfo. Print something else here?
    verbose( VB_SHMEM, "Process Mask: %s", mu_to_str(&process_mask) );
    verbose( VB_SHMEM, "Process Affinity Mask: %s", mu_to_str(&affinity_mask) );

    add_event(IDLE_CPUS_EVENT, idle_count);
}

void shmem_cpuinfo__finalize(void) {
    DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        int cpu;
        for (cpu = 0; cpu < node_size; cpu++) {
            if (shdata->node_info[cpu].owner == ME) {
                shdata->node_info[cpu].owner = NOBODY;
                if (shdata->node_info[cpu].guest == ME) {
                    shdata->node_info[cpu].guest = NOBODY;
                    update_cpu_stats(cpu, STATS_IDLE);
                }
                if (cpu_is_public_post_mortem
                        || CPU_ISSET( cpu, &dlb_mask)) {
                    shdata->node_info[cpu].state = CPU_LENT;
                } else {
                    shdata->node_info[cpu].state = CPU_DISABLED;
                }
            } else {
                // Free external CPUs that I may be using
                if (shdata->node_info[cpu].guest == ME) {
                    shdata->node_info[cpu].guest = NOBODY;
                    update_cpu_stats(cpu, STATS_IDLE);
                }
            }

            DLB_INSTR( if (is_idle(cpu)) idle_count++; )
        }
    }
    shmem_unlock(shm_handler);

    add_event(IDLE_CPUS_EVENT, idle_count);

    shmem_finalize(shm_handler);
    shm_handler = NULL;
}

/* Add cpu_mask to the Shared Mask
 * If the process originally owns the CPU:      State => CPU_LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
void shmem_cpuinfo__add_mask(const cpu_set_t *cpu_mask) {
    DLB_DEBUG( cpu_set_t freed_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        int cpu;
        for (cpu = 0; cpu < node_size; cpu++) {
            if (CPU_ISSET(cpu, cpu_mask)) {

                // If the CPU was mine, just change the state
                if (shdata->node_info[cpu].owner == ME) {
                    shdata->node_info[cpu].state = CPU_LENT;
                }

                // If am currently using the CPU, free it
                if (shdata->node_info[cpu].guest == ME) {
                    shdata->node_info[cpu].guest = NOBODY;
                    update_cpu_stats(cpu, STATS_IDLE);
                    DLB_DEBUG( CPU_SET(cpu, &freed_cpus); )
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if (is_idle(cpu)) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            }
        }
    }
    shmem_unlock(shm_handler);

    DLB_DEBUG( int size = CPU_COUNT(&freed_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    verbose(VB_SHMEM, "Lending %s", mu_to_str(&freed_cpus));
    verbose(VB_SHMEM, "Increasing %d Idle Threads (%d now)", size, post_size);
    verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    add_event(IDLE_CPUS_EVENT, idle_count);
}

/* Add cpu to the Shared Mask
 * If the process originally owns the CPU:      State => CPU_LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
void shmem_cpuinfo__add_cpu(int cpu) {
    DLB_DEBUG( cpu_set_t freed_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        // If the CPU was mine, just change the state
        if (shdata->node_info[cpu].owner == ME) {
            shdata->node_info[cpu].state = CPU_LENT;
        }

        // If am currently using the CPU, free it
        if (shdata->node_info[cpu].guest == ME) {
            shdata->node_info[cpu].guest = NOBODY;
            update_cpu_stats(cpu, STATS_IDLE);
            DLB_DEBUG( CPU_SET(cpu, &freed_cpus); )
        }

        // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
        int i;
        for (i = 0; i < node_size; i++) {
            if (is_idle(i)) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET(i, &idle_cpus); )
            }
        }
    }
    shmem_unlock(shm_handler);

    DLB_DEBUG( int size = CPU_COUNT(&freed_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    verbose(VB_SHMEM, "Lending %s", mu_to_str(&freed_cpus));
    verbose(VB_SHMEM, "Increasing %d Idle Threads (%d now)", size, post_size);
    verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    add_event(IDLE_CPUS_EVENT, idle_count);
}

/* Acquire some of the CPUs owned by the process
 * CPUs that owner == ME:           State => CPU_BUSY
 * CPUs that guest == NOBODY        Guest => ME
 */
void shmem_cpuinfo__recover_some_cpus(cpu_set_t *mask, int max_resources) {
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    DLB_INSTR( int idle_count = 0; )

    cpu_set_t recovered_cpus;
    CPU_ZERO(&recovered_cpus);
    max_resources = (max_resources) ? max_resources : INT_MAX;

    shmem_lock(shm_handler);
    {
        int cpu;
        for (cpu = 0; cpu < node_size && max_resources > 0; cpu++) {
            if (shdata->node_info[cpu].owner == ME && mask && !CPU_ISSET(cpu, mask)) {
                shdata->node_info[cpu].state = CPU_BUSY;
                CPU_SET(cpu, &recovered_cpus);
                --max_resources;

                if (shdata->node_info[cpu].guest == NOBODY) {
                    shdata->node_info[cpu].guest = ME;
                    update_cpu_stats(cpu, STATS_OWNED);
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if (is_idle(cpu)) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            }
        }
    }
    shmem_unlock(shm_handler);

    if (mask) {
        // If a mask was provided, add the recovered CPUs
        CPU_OR(mask, mask, &recovered_cpus);
    }

    DLB_DEBUG( int recovered = CPU_COUNT(&recovered_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    verbose(VB_SHMEM, "Decreasing %d Idle Threads (%d now)", recovered, post_size);
    verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    add_event(IDLE_CPUS_EVENT, idle_count);
}

/* Remove CPU from the Shared Mask
 * CPU if owner == ME:          State => CPU_BUSY
 * CPU if guest == NOBODY       Guest => ME
 */
void shmem_cpuinfo__recover_cpu(int cpu) {
    DLB_DEBUG( cpu_set_t recovered_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO(&recovered_cpus); )
    DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        if (shdata->node_info[cpu].owner == ME) {
            shdata->node_info[cpu].state = CPU_BUSY;
            if (shdata->node_info[cpu].guest == NOBODY) {
                shdata->node_info[cpu].guest = ME;
                update_cpu_stats(cpu, STATS_OWNED);
                DLB_DEBUG( CPU_SET(cpu, &recovered_cpus); )
            }
        }

        // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
        if (is_idle(cpu)) {
            DLB_INSTR( idle_count++; )
            DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
        }
    }
    shmem_unlock(shm_handler);

    DLB_DEBUG( int recovered = CPU_COUNT(&recovered_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
    verbose(VB_SHMEM, "Decreasing %d Idle Threads (%d now)", recovered, post_size);
    verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));

    add_event(IDLE_CPUS_EVENT, idle_count);
}

/* Remove non default CPUs that have been set as BUSY by their owner
 * or remove also CPUs that habe been set to DISABLED
 * Reclaimed CPUs:    Guest => Owner
 */
int shmem_cpuinfo__return_claimed(cpu_set_t *mask) {
    int returned = 0;

    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( cpu_set_t returned_cpus; )
    DLB_DEBUG( CPU_ZERO(&idle_cpus); )
    DLB_DEBUG( CPU_ZERO(&returned_cpus); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock(shm_handler);
    {
        int cpu;
        for (cpu = 0; cpu < node_size; cpu++) {
            if (CPU_ISSET(cpu, mask)
                    && shdata->node_info[cpu].owner != ME
                    && shdata->node_info[cpu].state != CPU_LENT) {
                if (shdata->node_info[cpu].guest == ME) {
                    shdata->node_info[cpu].guest = shdata->node_info[cpu].owner;
                    update_cpu_stats(cpu, STATS_OWNED);
                }
                returned++;
                CPU_CLR(cpu, mask);
                DLB_DEBUG( CPU_SET(cpu, &returned_cpus); )
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
            }
        }
    }
    shmem_unlock(shm_handler);
    if (returned > 0) {
        verbose(VB_SHMEM, "Giving back %d Threads (%s)", returned, mu_to_str(&returned_cpus));
        verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));
    }

    add_event(IDLE_CPUS_EVENT, idle_count);

    return returned;
}

/* Collect as many idle CPUs as indicated by max_resources
 * Affine CPUs have preference
 * Idle CPUs are the ones that (state == CPU_LENT) && (guest == NOBODY)
 * If successful:       Guest => ME
 */
int shmem_cpuinfo__collect_mask(cpu_set_t *mask, int max_resources) {
    int cpu;
    int collected1 = 0;
    int collected2 = 0;
    bool some_idle_cpu = false;

    // Since the process mask may change during the execution,
    // we need to compute the affinity_mask everytime
    // FIXME: Fast non-safe check to get the updated process mask
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    for (cpu = 0; cpu < node_size; cpu++) {
        if (shdata->node_info[cpu].owner == ME) {
            CPU_SET(cpu, &process_mask);
        }
    }
    cpu_set_t affinity_mask;
    mu_get_affinity_mask(&affinity_mask, &process_mask, MU_ANY_BIT);

    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO(&idle_cpus); )

    DLB_INSTR( int idle_count = 0; )

    // Fast non-safe check to get if there is some idle CPU
    for (cpu = 0; cpu < node_size; cpu++) {
        some_idle_cpu = is_idle(cpu);
        if (some_idle_cpu) break;
    }

    if (some_idle_cpu && max_resources > 0) {

        shmem_lock(shm_handler);
        {
            /* First Step: Retrieve affine cpus */
            for (cpu = 0; (cpu < node_size) && (max_resources > 0); cpu++) {
                if (CPU_ISSET(cpu, &affinity_mask)
                        && shdata->node_info[cpu].state == CPU_LENT
                        && shdata->node_info[cpu].guest == NOBODY) {
                    shdata->node_info[cpu].guest = ME;
                    update_cpu_stats(cpu, STATS_GUESTED);
                    CPU_SET(cpu, mask);
                    max_resources--;
                    collected1++;
                }
            }
            verbose(VB_SHMEM, "Getting %d affine Threads (%s)", collected1, mu_to_str(mask));

//            /* Second Step: Retrieve non-affine cpus, if needed */
//            cpu_set_t free_cpus; //mask with free cpus
//            cpu_set_t free_sockets; //mask with full free sockets
//
//            //first fill the free cpus mask
//            for ( cpu = 0; cpu < node_size; cpu++ ) {   
//                if ( shdata->node_info[cpu].state == CPU_LENT
//                        && shdata->node_info[cpu].guest == NOBODY ) {
//                    CPU_SET(cpu, &free_cpus);
//                }
//            }
//            //Get the full sockets
//            mu_get_affinity_mask( &free_sockets, &free_cpus, MU_ALL_BITS );
//
//            for ( cpu = 0; (cpu < node_size) && (max_resources > 0); cpu++ ) {
//                if ( CPU_ISSET( cpu, &free_sockets) ) {
//                    shdata->node_info[cpu].guest = ME;
//                    CPU_SET( cpu, mask );
//                    max_resources--;
//                    collected2++;
//                }
//            }

            //Original code to retrieve non affine CPUs
            for (cpu = 0; (cpu < node_size) && (max_resources > 0); cpu++) {
                if (shdata->node_info[cpu].state == CPU_LENT
                        && shdata->node_info[cpu].guest == NOBODY) {
                    shdata->node_info[cpu].guest = ME;
                    update_cpu_stats(cpu, STATS_GUESTED);
                    CPU_SET(cpu, mask);
                    max_resources--;
                    collected2++;
                }
            }
            verbose(VB_SHMEM, "Getting %d other Threads (%s)", collected2, mu_to_str(mask));

            // FIXME: Another loop, efficiency?
            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            for (cpu = 0; cpu < node_size; cpu++) {
                if (is_idle(cpu)) {
                    DLB_INSTR( idle_count++; )
                    DLB_DEBUG( CPU_SET(cpu, &idle_cpus); )
                }
            }
        }
        shmem_unlock(shm_handler);
    }

    int collected = collected1 + collected2;
    if (collected > 0) {
        DLB_DEBUG( int post_size = CPU_COUNT(&idle_cpus); )
        verbose(VB_SHMEM, "Clearing %d Idle Threads (%d left)", collected, post_size);
        verbose(VB_SHMEM, "Available mask: %s", mu_to_str(&idle_cpus));
        add_event(IDLE_CPUS_EVENT, idle_count);
    }
    return collected;
}

/* Return false if my CPU is being used by other process
 */
bool shmem_cpuinfo__is_cpu_borrowed(int cpu) {
    // If the CPU is not mine, skip check
    if (shdata->node_info[cpu].owner != ME) {
        return true;
    }

    // If the CPU is free, assign it to myself
    if (shdata->node_info[cpu].guest == NOBODY) {
        shmem_lock(shm_handler);
        if (shdata->node_info[cpu].guest == NOBODY) {
            shdata->node_info[cpu].guest = ME;
            update_cpu_stats(cpu, STATS_OWNED);
        }
        shmem_unlock(shm_handler);
    }

    return shdata->node_info[cpu].guest == ME;
}

/* Return true if the CPU is foreign and reclaimed
 */
bool shmem_cpuinfo__is_cpu_claimed(int cpu) {
    if (shdata->node_info[cpu].state == CPU_DISABLED) {
        return true;
    }

    return (shdata->node_info[cpu].owner != ME) && (shdata->node_info[cpu].state == CPU_BUSY);
}

/*Reset current mask to use my default cpus
  I.e: Release borrowed cpus and claim lend cpus
 */
int shmem_cpuinfo__reset_default_cpus(cpu_set_t *mask) {
    int cpu;
    int n=0;
    shmem_lock(shm_handler);
    for (cpu = 0; cpu < mu_get_system_size(); cpu++) {
        //CPU is mine --> claim it and set in my mask
        if (shdata->node_info[cpu].owner == ME) {
            shdata->node_info[cpu].state = CPU_BUSY;
            if (shdata->node_info[cpu].guest == NOBODY) {
                shdata->node_info[cpu].guest = ME;
                update_cpu_stats(cpu, STATS_OWNED);
            }
            CPU_SET(cpu, mask);
            n++;
        //CPU is not mine and I have it --> release it and clear from my mask
        } else if (CPU_ISSET(cpu, mask)) {
            if (shdata->node_info[cpu].guest == ME){
                shdata->node_info[cpu].guest = NOBODY;
                update_cpu_stats(cpu, STATS_IDLE);
            }
            CPU_CLR(cpu, mask);
        }
    }
    shmem_unlock(shm_handler);
    return n;

}

/*
  Acquire this cpu, wether it was mine or not
  Can have a problem if the cpu was borrowed by somebody else
  If force take it from the owner
 */
bool shmem_cpuinfo__acquire_cpu(int cpu, bool force) {

    bool acquired=true;

    shmem_lock(shm_handler);
    //cpu is mine -> Claim it
    if (shdata->node_info[cpu].owner == ME) {
        shdata->node_info[cpu].state = CPU_BUSY;
        // It is important to not modify the 'guest' field to remark that the CPU is claimed

    //cpu is not mine but is free -> take it
    } else if (shdata->node_info[cpu].state == CPU_LENT
            && shdata->node_info[cpu].guest == NOBODY) {
        shdata->node_info[cpu].guest = ME;
        update_cpu_stats(cpu, STATS_GUESTED);

    //cpus is not mine and the owner has recover it
    } else if (force) {
        //If force -> take it
        if (shdata->node_info[cpu].guest == shdata->node_info[cpu].owner) {
            shdata->node_info[cpu].guest = ME;
            update_cpu_stats(cpu, STATS_GUESTED);

        } else {
            //FIXME: Another process has borrowed it
            // or    the cpu is DISABLED in the system
            acquired=false;
        }
    } else {
        //No force -> nothing
        acquired=false;
    }

    // add_event(IDLE_CPUS_EVENT, idle_count);
    shmem_unlock(shm_handler);
    return acquired;
}

/* Update CPU ownership according to the new process mask.
 * To avoid collisions, we only release the ownership if we still own it
 */
void shmem_cpuinfo__update_ownership(const cpu_set_t* process_mask) {
    shmem_lock(shm_handler);
    int cpu;
    for (cpu = 0; cpu < mu_get_system_size(); cpu++) {
        if (CPU_ISSET(cpu, process_mask)) {
            // CPU ownership should be mine
            if (shdata->node_info[cpu].owner != ME) {
                // Steal CPU
                shdata->node_info[cpu].owner = ME;
                shdata->node_info[cpu].guest = ME;
                shdata->node_info[cpu].state = CPU_BUSY;
                update_cpu_stats(cpu, STATS_OWNED);
                verbose(VB_SHMEM, "Acquiring ownership of CPU %d", cpu);
            }
        } else if (shdata->node_info[cpu].owner == ME) {
            // Release CPU ownership
            shdata->node_info[cpu].owner = NOBODY;
            shdata->node_info[cpu].state = CPU_DISABLED;
            update_cpu_stats(cpu, STATS_IDLE);
            verbose(VB_SHMEM, "Releasing ownership of CPU %d", cpu);
        }
    }
    shmem_unlock(shm_handler);
}

/* External Functions
 * These functions are intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize the shared memory
 */

static shmem_handler_t *shm_ext_handler = NULL;

void shmem_cpuinfo_ext__init(void) {
    // Protect multiple initialization
    if (shm_ext_handler != NULL) {
        return;
    }

    node_size = mu_get_system_size();
    shm_ext_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size, "cpuinfo");
}

void shmem_cpuinfo_ext__finalize(void) {
    shmem_finalize(shm_ext_handler);
    shm_ext_handler = NULL;
}

int shmem_cpuinfo_ext__getnumcpus(void) {
    return mu_get_system_size();
}

float shmem_cpuinfo_ext__getcpustate(int cpu, stats_state_t state) {
    float usage;
    shmem_handler_t *handler;

    // Initialize only if the user didn't do the Initialization
    if (shm_ext_handler == NULL) {
        handler = shmem_init((void**)&shdata,
                sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size, "cpuinfo");
    } else {
        handler = shm_ext_handler;
    }

    shmem_lock(handler);
    {
        usage = getcpustate(cpu, state);
    }
    shmem_unlock(handler);

    // Finalize only if the user didn't do the Initialization
    if (shm_ext_handler == NULL) {
        shmem_finalize(handler);
    }

    return usage;
}

void shmem_cpuinfo_ext__print_info(void) {
    node_size = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_handler_t *handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(cpuinfo_t)*node_size, "cpuinfo");
    cpuinfo_t node_info_copy[node_size];

    shmem_lock(handler);
    {
        memcpy(node_info_copy, shdata->node_info, sizeof(cpuinfo_t)*node_size);
    }
    shmem_unlock(handler);
    shmem_finalize(handler);

    char owners[512] = "OWNERS: ";
    char guests[512] = "GUESTS: ";
    char states[512] = "STATES: ";
    char *o = owners+8;
    char *g = guests+8;
    char *s = states+8;
    int cpu;
    for (cpu=0; cpu<node_size; cpu++) {
        o += snprintf(o, 8, "%d, ", node_info_copy[cpu].owner);
        g += snprintf(g, 8, "%d, ", node_info_copy[cpu].guest);
        s += snprintf(s, 8, "%d, ", node_info_copy[cpu].state);
    }

    info0(owners);
    info0(guests);
    info0(states);
    info0("States Legend: DISABLED=%d, BUSY=%d, LENT=%d", CPU_DISABLED, CPU_BUSY, CPU_LENT);

    // TODO use info
    info0("CPU Statistics:");
    for (cpu = 0; cpu < node_size; ++cpu) {
        fprintf(stdout, "CPU %d: OWNED(%.2f%%), GUESTED(%.2f%%), IDLE(%.2f%%)\n",
                cpu,
                getcpustate(cpu, STATS_OWNED)*100,
                getcpustate(cpu, STATS_GUESTED)*100,
                getcpustate(cpu, STATS_IDLE)*100);
    }
}

/*** Helper functions, the shm lock must have been acquired beforehand ***/
static inline bool is_idle(int cpu) {
    return (shdata->node_info[cpu].state == CPU_LENT) && (shdata->node_info[cpu].guest == NOBODY);
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

static float getcpustate(int cpu, stats_state_t state) {
    struct timespec now;
    get_time_coarse(&now);
    int64_t total = timespec_diff(&shdata->initial_time, &now);
    int64_t acc = shdata->node_info[cpu].acc_time[state];
    float percentage = (float)acc/total;
    ensure(percentage >= -0.000001f && percentage <= 1.000001f,
                "Percentage out of bounds");
    return percentage;
}
/*** End of helper functions ***/
