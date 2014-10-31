/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "shmem.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "support/utils.h"
#include "support/mask_utils.h"

/* NOTE on default values:
 * The shared memory will be initializated to 0 when created,
 * thus all default values should represent 0
 * A CPU, by default, is DISABLED and owned by NOBODY
 */

#define NOBODY 0
#define ME getpid()

typedef pid_t spid_t;  // Sub-process ID

typedef enum {
    DISABLED = 0,        // Not owned by any process nor part of the DLB mask
    BUSY,
    LENT
} cpu_state_t;

typedef struct {
    spid_t      owner;   // Original owner, useful to resolve reclaimed CPUs
    spid_t      guest;   // Current user of the CPU
    cpu_state_t state;
} cpu_info_t;

typedef struct {
    cpu_info_t node_info[0];
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;   // default mask of the process
static cpu_set_t affinity_mask;  // affinity mask of the process
static cpu_set_t dlb_mask;       // CPUs not owned by any process but usable by others
static int cpus_node;
static bool cpu_is_public_post_mortem = false;

static inline bool is_idle( int cpu ) {
    return (shdata->node_info[cpu].state == LENT) && (shdata->node_info[cpu].guest == NOBODY);
}

void shmem_cpuarray__init( const cpu_set_t *cpu_set ) {
    mu_init();
    mu_parse_mask( "LB_MASK", &dlb_mask );
    memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
    mu_get_affinity_mask( &affinity_mask, &default_mask, MU_ANY_BIT );
    cpus_node = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_init( &shdata, sizeof(shdata_t) + sizeof(cpu_info_t)*cpus_node );

    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        // Check first that my default_mask is not already owned
        for ( cpu = 0; cpu < cpus_node; cpu++ )
            if ( CPU_ISSET( cpu, &default_mask ) && shdata->node_info[cpu].owner != NOBODY ) {
                shmem_unlock();
                fatal0( "Another process in the same node is using one of your cpus\n" );
            }

        for ( cpu = 0; cpu < cpus_node; cpu++ ) {
            // Add my mask info
            if ( CPU_ISSET( cpu, &default_mask ) ) {
                shdata->node_info[cpu].owner = ME;
                shdata->node_info[cpu].state = BUSY;
                // It could be that my cpu was being used by anybody else if it was set in the DLB mask
                if ( shdata->node_info[cpu].guest == NOBODY ) {
                    shdata->node_info[cpu].guest = ME;
                }
            }
            // Add DLB mask info
            if ( CPU_ISSET( cpu, &dlb_mask )  &&
                    shdata->node_info[cpu].owner == NOBODY &&
                    shdata->node_info[cpu].state == DISABLED ) {
                shdata->node_info[cpu].state = LENT;
            }

            // Look for Idle CPUs
            DLB_INSTR( if (is_idle(cpu)) idle_count++; )
            }
    }
    shmem_unlock();

    debug_shmem( "Default Mask: %s\n", mu_to_str(&default_mask) );
    debug_shmem( "Default Affinity Mask: %s\n", mu_to_str(&affinity_mask) );

    add_event( IDLE_CPUS_EVENT, idle_count );
}

void shmem_cpuarray__finalize( void ) {
    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        for ( cpu = 0; cpu < cpus_node; cpu++ ) {
            if ( CPU_ISSET( cpu, &default_mask ) ) {
                shdata->node_info[cpu].owner = NOBODY;
                if ( shdata->node_info[cpu].guest == ME ) {
                    shdata->node_info[cpu].guest = NOBODY;
                }
                if ( cpu_is_public_post_mortem ) {
                    shdata->node_info[cpu].state = LENT;
                } else if ( CPU_ISSET( cpu, &dlb_mask ) ) {
                    shdata->node_info[cpu].state = LENT;
                } else {
                    shdata->node_info[cpu].state = DISABLED;
                }
            } else {
                // Free external CPUs that may I be using
                if ( shdata->node_info[cpu].guest == ME ) {
                    shdata->node_info[cpu].guest = NOBODY;
                }
            }

            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
            }
        }
    }
    shmem_unlock();

    add_event( IDLE_CPUS_EVENT, idle_count );

    shmem_finalize();
    mu_finalize();
}

/* Add cpu_mask to the Shared Mask
 * If the process originally owns the CPU:      State => LENT
 * If the process is currently using the CPU:   Guest => NOBODY
 */
void shmem_cpuarray__add_mask( const cpu_set_t *cpu_mask ) {
    DLB_DEBUG( cpu_set_t freed_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &freed_cpus ); )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        for ( cpu = 0; cpu < cpus_node; cpu++ ) {
            if ( CPU_ISSET( cpu, cpu_mask ) ) {

                // If the CPU was mine, just change the state
                if ( CPU_ISSET( cpu, &default_mask ) ) {
                    shdata->node_info[cpu].state = LENT;
                }

                // If am currently using the CPU, free it
                if ( shdata->node_info[cpu].guest == ME ) {
                    shdata->node_info[cpu].guest = NOBODY;
                    DLB_DEBUG( CPU_SET( cpu, &freed_cpus ); )
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET( cpu, &idle_cpus ); )
            }
        }
    }
    shmem_unlock();

    DLB_DEBUG( int size = CPU_COUNT( &freed_cpus ); )
    DLB_DEBUG( int post_size = CPU_COUNT( &idle_cpus); )
    debug_shmem( "Lending %s\n", mu_to_str(&freed_cpus) );
    debug_shmem( "Increasing %d Idle Threads (%d now)\n", size, post_size );
    debug_shmem( "Available mask: %s\n", mu_to_str(&idle_cpus) );

    add_event( IDLE_CPUS_EVENT, idle_count );
}

/* Remove the process default mask from the Shared Mask
 * CPUs from default_mask:          State => BUSY
 * CPUs that also have no guest:    Guest => ME
 */
const cpu_set_t* shmem_cpuarray__recover_defmask( void ) {
    DLB_DEBUG( cpu_set_t recovered_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &recovered_cpus ); )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        for ( cpu = 0; cpu < cpus_node; cpu++ ) {
            if ( CPU_ISSET( cpu, &default_mask ) ) {
                shdata->node_info[cpu].state = BUSY;
                if ( shdata->node_info[cpu].guest == NOBODY ) {
                    shdata->node_info[cpu].guest = ME;
                    DLB_DEBUG( CPU_SET( cpu, &recovered_cpus ); )
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET( cpu, &idle_cpus ); )
            }
        }
    }
    shmem_unlock();

    DLB_DEBUG( int recovered = CPU_COUNT( &recovered_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT( &idle_cpus); )
    debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", recovered, post_size );
    debug_shmem ( "Available mask: %s\n", mu_to_str(&idle_cpus) );

    add_event( IDLE_CPUS_EVENT, idle_count );

    return &default_mask;
}

/* Remove part of the process default mask from the Shared Mask
 * CPUs from default_mask:          State => BUSY
 * CPUs that also have no guest:    Guest => ME
 */
void shmem_cpuarray__recover_some_defcpus( cpu_set_t *mask, int max_resources ) {
    DLB_DEBUG( cpu_set_t recovered_cpus; )
    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &recovered_cpus ); )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        for ( cpu = 0; (cpu < cpus_node) && (max_resources > 0); cpu++ ) {
            if ( (CPU_ISSET(cpu, &default_mask)) && (!CPU_ISSET(cpu, mask)) ) {
                shdata->node_info[cpu].state = BUSY;
                CPU_SET( cpu, mask );
                max_resources--;
                if ( shdata->node_info[cpu].guest == NOBODY ) {
                    shdata->node_info[cpu].guest = ME;
                    DLB_DEBUG( CPU_SET( cpu, &recovered_cpus ); )
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET( cpu, &idle_cpus ); )
            }
        }
    }
    shmem_unlock();

    DLB_DEBUG( int recovered = CPU_COUNT( &recovered_cpus); )
    DLB_DEBUG( int post_size = CPU_COUNT( &idle_cpus); )
    debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", recovered, post_size );
    debug_shmem ( "Available mask: %s\n", mu_to_str(&idle_cpus) );

    add_event( IDLE_CPUS_EVENT, idle_count );
}


/* Remove non default CPUs that have been set as BUSY by their owner
 * or remove also CPUs that habe been set DISABLED
 * Reclaimed CPUs:    Guest => Owner
 */
int shmem_cpuarray__return_claimed ( cpu_set_t *mask ) {
    int returned = 0;

    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( cpu_set_t returned_cpus; )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )
    DLB_DEBUG( CPU_ZERO( &returned_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    shmem_lock();
    {
        int cpu;
        for ( cpu = 0; cpu < cpus_node; cpu++ ) {
            fatal_cond0( CPU_ISSET( cpu, mask ) && shdata->node_info[cpu].guest != ME,
                         "Current mask and Shared Memory information differ\n" );
            if ( CPU_ISSET( cpu, mask ) ) {
                if ( (shdata->node_info[cpu].owner != ME     &&
                        shdata->node_info[cpu].guest == ME     &&
                        shdata->node_info[cpu].state == BUSY)  ||
                        (shdata->node_info[cpu].state == DISABLED) ) {
                    shdata->node_info[cpu].guest = shdata->node_info[cpu].owner;
                    returned++;
                    CPU_CLR( cpu, mask );
                    DLB_DEBUG( CPU_SET( cpu, &returned_cpus ); )
                }
            }

            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            if ( is_idle(cpu) ) {
                DLB_INSTR( idle_count++; )
                DLB_DEBUG( CPU_SET( cpu, &idle_cpus ); )
            }
        }
    }
    shmem_unlock();
    if ( returned > 0 ) {
        debug_shmem ( "Giving back %d Threads (%s)\n", returned, mu_to_str(&returned_cpus) );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&idle_cpus) );
    }

    add_event( IDLE_CPUS_EVENT, idle_count );

    return returned;
}

/* Collect as many idle CPUs as indicated by max_resources
 * Affine CPUs have preference
 * Idle CPUs are the ones that (state == LENT) && (guest == NOBODY)
 * If successful:       Guest => ME
 */
int shmem_cpuarray__collect_mask ( cpu_set_t *mask, int max_resources ) {
    int cpu;
    int collected1 = 0;
    int collected2 = 0;
    bool some_idle_cpu = false;

    DLB_DEBUG( cpu_set_t idle_cpus; )
    DLB_DEBUG( CPU_ZERO( &idle_cpus ); )

    DLB_INSTR( int idle_count = 0; )

    // Fast non-safe check to get if there is some idle CPU
    for ( cpu = 0; cpu < cpus_node; cpu++ ) {
        some_idle_cpu = is_idle( cpu );
        if ( some_idle_cpu ) { break; }
    }

    if ( some_idle_cpu && max_resources > 0) {

        shmem_lock();
        {
            /* First Step: Retrieve affine cpus */
            for ( cpu = 0; (cpu < cpus_node) && (max_resources > 0); cpu++ ) {
                if ( CPU_ISSET( cpu, &affinity_mask )        &&
                        shdata->node_info[cpu].state == LENT   &&
                        shdata->node_info[cpu].guest == NOBODY ) {
                    shdata->node_info[cpu].guest = ME;
                    CPU_SET( cpu, mask );
                    max_resources--;
                    collected1++;
                }
            }
            debug_shmem ( "Getting %d affine Threads (%s)\n", collected1, mu_to_str(mask) );

            /* Second Step: Retrieve non-affine cpus, if needed */
            for ( cpu = 0; (cpu < cpus_node) && (max_resources > 0); cpu++ ) {
                if ( shdata->node_info[cpu].state == LENT    &&
                        shdata->node_info[cpu].guest == NOBODY ) {
                    shdata->node_info[cpu].guest = ME;
                    CPU_SET( cpu, mask );
                    max_resources--;
                    collected2++;
                }
            }
            debug_shmem ( "Getting %d other Threads (%s)\n", collected2, mu_to_str(mask) );

            // FIXME: Another loop, efficiency?
            // Look for Idle CPUs, only in DEBUG or INSTRUMENTATION
            for ( cpu = 0; cpu < cpus_node; cpu++ ) {
                if ( is_idle(cpu) ) {
                    DLB_INSTR( idle_count++; )
                    DLB_DEBUG( CPU_SET( cpu, &idle_cpus ); )
                }
            }
        }
        shmem_unlock();
    }

    int collected = collected1 + collected2;
    if ( collected > 0 ) {
        DLB_DEBUG( int post_size = CPU_COUNT( &idle_cpus ); )
        debug_shmem ( "Clearing %d Idle Threads (%d left)\n", collected, post_size );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&idle_cpus) );
        add_event( IDLE_CPUS_EVENT, idle_count );
    }
    return collected;
}

/* Return false if my CPU is being used by other process
 */
bool shmem_cpuarray__is_cpu_borrowed ( int cpu ) {
    // If the CPU is not mine, skip check
    if ( !(CPU_ISSET(cpu, &default_mask)) ) {
        return true;
    }

    // If the CPU is free, assign it to myself
    if ( shdata->node_info[cpu].guest == NOBODY ) {
        shdata->node_info[cpu].guest = ME;
    }

    return shdata->node_info[cpu].guest == ME;
}

/* Return true if the CPU is foreign and reclaimed
 */
bool shmem_cpuarray__is_cpu_claimed( int cpu ) {
    if ( shdata->node_info[cpu].state == DISABLED ) {
        return true;
    }

    return (shdata->node_info[cpu].owner != ME) && (shdata->node_info[cpu].state == BUSY);
}


/* This function is intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize the shared memory
 */
void shmem_cpuarray__print_info( void ) {
    mu_init();
    cpus_node = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_init( &shdata, sizeof(shdata_t) + sizeof(cpu_info_t)*cpus_node );
    cpu_info_t node_info_copy[cpus_node];

    shmem_lock();
    {
        memcpy( node_info_copy, shdata->node_info, sizeof(cpu_info_t)*cpus_node );
    }
    shmem_unlock();
    shmem_finalize();

    char owners[512] = "OWNERS: ";
    char guests[512] = "GUESTS: ";
    char states[512] = "STATES: ";
    char *o = owners+8;
    char *g = guests+8;
    char *s = states+8;
    int cpu;
    for ( cpu=0; cpu<cpus_node; cpu++ ) {
        o += snprintf( o, 8, "%d, ", node_info_copy[cpu].owner );
        g += snprintf( g, 8, "%d, ", node_info_copy[cpu].guest );
        s += snprintf( s, 8, "%d, ", node_info_copy[cpu].state );
    }
    *o = '\n';
    *g = '\n';
    *s = '\n';

    debug_basic_info0( owners );
    debug_basic_info0( guests );
    debug_basic_info0( states );
    debug_basic_info0( "States Legend: DISABLED=%d, BUSY=%d, LENT=%d\n", DISABLED, BUSY, LENT );
}
