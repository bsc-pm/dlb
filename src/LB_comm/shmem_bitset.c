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
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "shmem.h"
#include "support/tracing.h"
#include "support/globals.h"
#include "support/debug.h"
#include "support/utils.h"
#include "support/mask_utils.h"

typedef struct {
    /*
     * When a process gives their cpus, they are added to both sets.
     * When a process recovers their cpus, they are removed from both sets.
     * When a process collects cpus, they are removed only from avail_cpus,
     *    and if it owns cpus not present in given_cpus it must give them back.
     *    (Its own cpus are excluded, of course)
     */
    cpu_set_t given_cpus; // set of given cpus. Only modified by its own process.
    cpu_set_t avail_cpus; // set of available cpus at the moment
    cpu_set_t not_borrowed_cpus; // set of cpus not used by somebody not his owner

    cpu_set_t check_mask; // To check whether the masks of
    // the node processes are disjoint
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;   // default mask of the process

static bool has_shared_mask(void);

void shmem_bitset__init( const cpu_set_t *cpu_set ) {
    shmem_init( &shdata, sizeof(shdata_t) );
    add_event( IDLE_CPUS_EVENT, 0 );

    memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
    mu_init();
    cpu_set_t affinity_mask;
    // Get the parent mask of any bit present in the default mask
    mu_get_affinity_mask( &affinity_mask, &default_mask, MU_ANY_BIT );

    // We have to check the shared_mask first before screwing up the shdata
    fatal_cond( has_shared_mask(), "Another process in the same node is using one of your cpus\n" );

    shmem_lock();
    {
        int i;
        for ( i = 0; i < mu_get_system_size(); i++ ) {
            if ( CPU_ISSET( i, &default_mask ) ) {
                CPU_CLR( i, &(shdata->given_cpus) );
                CPU_CLR( i, &(shdata->avail_cpus) );
                CPU_SET( i, &(shdata->not_borrowed_cpus) );
            }
        }
    }
    shmem_unlock();

    debug_shmem( "Default Mask: %s\n", mu_to_str(&default_mask) );
    debug_shmem( "Default Affinity Mask: %s\n", mu_to_str(&affinity_mask) );
}

void shmem_bitset__finalize( void ) {
    // In non-MPI libs, we have to remove our cpus from the shared memory, since it'll survive the process
#ifndef MPI_LIB
    shmem_lock();
    {
        int i;
        for ( i = 0; i < mu_get_system_size(); i++ ) {
            if ( CPU_ISSET( i, &default_mask ) ) {
                CPU_CLR( i, &(shdata->given_cpus) );
                CPU_CLR( i, &(shdata->avail_cpus) );
                CPU_SET( i, &(shdata->not_borrowed_cpus) );
                CPU_CLR( i, &(shdata->check_mask) );
            }
        }
    }
    shmem_unlock();
#endif

    shmem_finalize();
    mu_finalize();
}

void shmem_bitset__add_mask( const cpu_set_t *cpu_set ) {
    cpu_set_t cpus_to_give;
    cpu_set_t cpus_to_free;
    cpu_set_t cpus_not_mine;

    shmem_lock();
    {
        // We only add the owned cpus to given_cpus
        CPU_AND( &cpus_to_give, &default_mask, cpu_set );
        CPU_OR( &(shdata->given_cpus), &(shdata->given_cpus), &cpus_to_give );

        //Mark as not borrowed the cpus that I had borrowed
        CPU_XOR( &cpus_not_mine, &default_mask, cpu_set );
        CPU_AND( &cpus_not_mine, &cpus_not_mine, cpu_set );
        CPU_OR( &(shdata->not_borrowed_cpus), &(shdata->not_borrowed_cpus), &cpus_not_mine);

        // We only free the cpus that are present in given_cpus
        CPU_AND( &cpus_to_free, &(shdata->given_cpus), cpu_set );
        debug_shmem ( "Lending %s\n", mu_to_str(&cpus_to_free) );

        // Remove from the set the cpus that are being borrowed
        CPU_AND( &cpus_to_free, &(shdata->not_borrowed_cpus), &cpus_to_free);
        debug_shmem ( "Lending2 %s\n", mu_to_str(&cpus_to_free) );
        CPU_OR( &(shdata->avail_cpus), &(shdata->avail_cpus), &cpus_to_free );


        DLB_DEBUG( int size = CPU_COUNT( &cpus_to_free ); )
        DLB_DEBUG( int post_size = CPU_COUNT( &(shdata->avail_cpus) ); )
        debug_shmem ( "Increasing %d Idle Threads (%d now)\n", size, post_size );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) );
        debug_shmem ( "Not borrowed %s\n", mu_to_str(&(shdata->not_borrowed_cpus)) );
    }
    shmem_unlock();

    add_event( IDLE_CPUS_EVENT, CPU_COUNT( &(shdata->avail_cpus) ) );
}

void shmem_bitset__add_cpu( int cpu ) {
    cpu_set_t mask;
    CPU_ZERO( &mask );
    CPU_SET( cpu, &mask );
    shmem_bitset__add_mask( &mask );
}

const cpu_set_t* shmem_bitset__recover_defmask( void ) {
    shmem_lock();
    {
        DLB_DEBUG( int prev_size = CPU_COUNT( &(shdata->avail_cpus) ); )
        int i;
        for ( i = 0; i < mu_get_system_size(); i++ ) {
            if ( CPU_ISSET(i, &default_mask) ) {
                CPU_CLR( i, &(shdata->given_cpus) );
                CPU_CLR( i, &(shdata->avail_cpus) );
            }
        }
        DLB_DEBUG( int post_size = CPU_COUNT( &(shdata->avail_cpus) ); )
        debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", prev_size - post_size, post_size );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
    }
    shmem_unlock();

    add_event( IDLE_CPUS_EVENT, CPU_COUNT( &(shdata->avail_cpus) ) );

    return &default_mask;
}

void shmem_bitset__recover_some_defcpus( cpu_set_t *mask, int max_resources ) {
    shmem_lock();
    {
        DLB_DEBUG( int prev_size = CPU_COUNT( &(shdata->avail_cpus) ); )
        int i;
        for ( i = 0; i < mu_get_system_size() && max_resources>0; i++ ) {
            if ( (CPU_ISSET(i, &default_mask)) && (!CPU_ISSET(i, mask)) ) {
                CPU_CLR( i, &(shdata->given_cpus) );
                CPU_CLR( i, &(shdata->avail_cpus) );
                CPU_SET( i, mask );
                max_resources--;
            }
        }
        assert(max_resources==0);
        DLB_DEBUG( int post_size = CPU_COUNT( &(shdata->avail_cpus) ); )
        debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", prev_size - post_size, post_size );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
    }
    shmem_unlock();

    add_event( IDLE_CPUS_EVENT, CPU_COUNT( &(shdata->avail_cpus) ) );
}

int shmem_bitset__return_claimed ( cpu_set_t *mask ) {
    int i;
    int returned = 0;
    cpu_set_t slaves_mask;

    DLB_DEBUG( cpu_set_t returned_cpus; )
    DLB_DEBUG( CPU_ZERO( &returned_cpus ); )

    CPU_XOR( &slaves_mask, mask, &default_mask );
    shmem_lock();
    {
        // Remove extra-cpus (slaves_mask) that have been removed from given_cpus
        for ( i=0; i<mu_get_system_size(); i++ ) {
            if ( CPU_ISSET( i, mask ) && CPU_ISSET( i, &slaves_mask ) && !CPU_ISSET( i, &(shdata->given_cpus) ) ) {
                CPU_CLR( i, mask );
                CPU_SET( i, &(shdata->not_borrowed_cpus)); //Mark that I returned the borrowed cpu
                returned++;
                DLB_DEBUG( CPU_SET( i, &returned_cpus ); )
            }
        }
    }
    shmem_unlock();
    if ( returned > 0 ) {
        debug_shmem ( "Giving back %d Threads\n", returned );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
        debug_shmem ( "Not borrowed %s\n", mu_to_str(&(shdata->not_borrowed_cpus)) );
    }

    return returned;
}

int shmem_bitset__collect_mask ( cpu_set_t *mask, int max_resources ) {
    int i;
    int collected = 0;
    int size = CPU_COUNT( &(shdata->avail_cpus) );

    if ( size > 0 && max_resources > 0) {

        cpu_set_t candidates_mask;
        cpu_set_t affinity_mask;
        mu_get_affinity_mask( &affinity_mask, mask, MU_ANY_BIT );
        int aux=CPU_COUNT(mask);
        shmem_lock();
        {
            /* First Step: Retrieve affine cpus */
            CPU_AND( &candidates_mask, &affinity_mask, &(shdata->avail_cpus) );
            for ( i=0; i<mu_get_system_size() && max_resources>0; i++ ) {
                if ( CPU_ISSET( i, &candidates_mask ) ) {
                    assert(!CPU_ISSET(i, mask));
                    CPU_CLR( i, &(shdata->avail_cpus) );
                    CPU_SET( i, mask );
                    if (!CPU_ISSET(i, &default_mask)) { //If the cpu is not mine
                        CPU_CLR(i, &(shdata->not_borrowed_cpus));    //Mark as borrowed cpu
                    }
                    max_resources--;
                    collected++;
                }
            }
            debug_shmem ( "Getting %d affine Threads (%s)\n", collected, mu_to_str(mask) );

            assert(collected==CPU_COUNT(mask)-aux);
            /* Second Step: Retrieve non-affine cpus, if needed */
            if ( size-collected > 0 && max_resources > 0 ) {

                if ( _priorize_locality ) {
                    mu_get_affinity_mask( &affinity_mask, &(shdata->given_cpus), MU_ALL_BITS );
                    CPU_AND( &candidates_mask, &affinity_mask, &(shdata->avail_cpus) );
                } else {
                    memcpy( &candidates_mask, &(shdata->avail_cpus), sizeof(cpu_set_t) );
                }

                for ( i=0; i<mu_get_system_size() && max_resources>0; i++ ) {
                    if ( CPU_ISSET( i, &candidates_mask ) ) {
                        assert(!CPU_ISSET(i, mask));
                        CPU_CLR( i, &(shdata->avail_cpus) );
                        CPU_SET( i, mask );
                        if (!CPU_ISSET(i, &default_mask)) { //If the cpu is not mine
                            CPU_CLR(i, &(shdata->not_borrowed_cpus));    //Mark as borrowed cpu
                        }
                        max_resources--;
                        collected++;
                    }
                }
                debug_shmem ( "Getting %d other Threads (%s)\n", collected, mu_to_str(mask) );
            }
        }
        shmem_unlock();
    }

    if ( collected > 0 ) {
        debug_shmem ( "Clearing %d Idle Threads (%d left)\n", collected,  size - collected );
        debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
        debug_shmem ( "Not borrowed %s\n", mu_to_str(&(shdata->not_borrowed_cpus)) );
        add_event( IDLE_CPUS_EVENT, size - collected );
    }
    return collected;
}

bool shmem_bitset__is_cpu_borrowed ( int cpu ) {
    //If the cpu is mine just check that it is not borrowed
    if (CPU_ISSET(cpu, &default_mask)) {
        return CPU_ISSET(cpu, &(shdata->not_borrowed_cpus));
    } else {
        return true;
    }
}

bool shmem_bitset__is_cpu_claimed( int cpu ) {
    //Just check if my cpu is claimed
    if (!CPU_ISSET( cpu, &default_mask ) && !CPU_ISSET( cpu, &(shdata->given_cpus) ) ) {
        return true;
    } else {
        return false;
    }
}


/* This function is intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize with the shared mem
 */
void shmem_bitset__print_info( void ) {
    shmem_init( &shdata, sizeof(shdata_t) );

    cpu_set_t given, avail, not_borrowed, check;

    shmem_lock();
    {
        memcpy( &given, &(shdata->given_cpus), sizeof(cpu_set_t) );
        memcpy( &avail, &(shdata->avail_cpus), sizeof(cpu_set_t) );
        memcpy( &not_borrowed, &(shdata->not_borrowed_cpus), sizeof(cpu_set_t) );
        memcpy( &check, &(shdata->check_mask), sizeof(cpu_set_t) );
    }
    shmem_unlock();
    shmem_finalize();

    debug_basic_info0( "Given CPUs:        %s\n", mu_to_str( &given ) );
    debug_basic_info0( "Available CPUs:    %s\n", mu_to_str( &avail ) );
    debug_basic_info0( "Not borrowed CPUs: %s\n", mu_to_str( &not_borrowed ) );
    debug_basic_info0( "Running CPUs:      %s\n", mu_to_str( &check ) );
}

static bool has_shared_mask( void ) {
    bool shared;
    cpu_set_t intxn_mask; // intersection mask
    CPU_ZERO( &intxn_mask );
    shmem_lock();
    {
        CPU_AND( &intxn_mask, &(shdata->check_mask), &default_mask );
        if (CPU_COUNT( &intxn_mask ) > 0 ) {
            shared = true;
        } else {
            CPU_OR( &(shdata->check_mask), &(shdata->check_mask), &default_mask );
            shared = false;
        }
    }
    shmem_unlock();

    return shared;
}
