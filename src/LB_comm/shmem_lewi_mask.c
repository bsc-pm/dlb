/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;   // default mask of the process

void shmem_lewi_mask_init( cpu_set_t *cpu_set )
{
   shmem_init( &shdata, sizeof(shdata_t) );
   add_event( IDLE_CPUS_EVENT, 0 );
   if ( _process_id == 0 ) {
      CPU_ZERO( &(shdata->given_cpus) );
      CPU_ZERO( &(shdata->avail_cpus) );
   }

   memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
   mu_init();
   cpu_set_t affinity_mask;
   // Get the parent mask of any bit present in the default mask
   mu_get_affinity_mask( &affinity_mask, &default_mask, MU_ANY_BIT );
   debug_shmem( "Default Mask: %s\n", mu_to_str(&default_mask) );
   debug_shmem( "Default Affinity Mask: %s\n", mu_to_str(&affinity_mask) );
}

void shmem_lewi_mask_finalize( void )
{
   shmem_finalize();
   mu_finalize();
}

void shmem_lewi_mask_add_mask( cpu_set_t *cpu_set )
{
   int post_size;
   cpu_set_t cpus_to_give;
   cpu_set_t cpus_to_free;

   shmem_lock();
   {
      // We only add the owned cpus to given_cpus
      CPU_AND( &cpus_to_give, &default_mask, cpu_set );
      CPU_OR( &(shdata->given_cpus), &(shdata->given_cpus), &cpus_to_give );

      // We only free the cpus that are present in given_cpus
      CPU_AND( &cpus_to_free, &(shdata->given_cpus), cpu_set );
      CPU_OR( &(shdata->avail_cpus), &(shdata->avail_cpus), &cpus_to_free );

      post_size = CPU_COUNT( &(shdata->avail_cpus) );
      DLB_DEBUG( int size = CPU_COUNT( &cpus_to_free ); )
      debug_shmem ( "Increasing %d Idle Threads (%d now)\n", size, post_size );
      debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) );
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );
}

cpu_set_t* shmem_lewi_mask_recover_defmask( void )
{
   int post_size, i;

   shmem_lock();
   {
      DLB_DEBUG( int prev_size = CPU_COUNT( &(shdata->avail_cpus) ); )
      for ( i = 0; i < mu_get_system_size(); i++ ) {
         if ( CPU_ISSET(i, &default_mask) ) {
            CPU_CLR( i, &(shdata->given_cpus) );
            CPU_CLR( i, &(shdata->avail_cpus) );
         }
      }
      post_size = CPU_COUNT( &(shdata->avail_cpus) );
      debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", prev_size - post_size, post_size );
      debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );

   return &default_mask;
}

void shmem_lewi_mask_recover_some_defcpus( cpu_set_t *mask, int max_resources )
{
   int post_size, i;

   shmem_lock();
   {
      DLB_DEBUG( int prev_size = CPU_COUNT( &(shdata->avail_cpus) ); )
      for ( i = 0; i < mu_get_system_size() && max_resources>0; i++ ) {
         if ( CPU_ISSET(i, &default_mask) ) {
            CPU_CLR( i, &(shdata->given_cpus) );
            CPU_CLR( i, &(shdata->avail_cpus) );
	    CPU_SET( i, mask );
	    max_resources--;
         }
      }
      post_size = CPU_COUNT( &(shdata->avail_cpus) );
      debug_shmem ( "Decreasing %d Idle Threads (%d now)\n", prev_size - post_size, post_size );
      debug_shmem ( "Available mask: %s\n", mu_to_str(&(shdata->avail_cpus)) ) ;
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );

   return;
}

int shmem_lewi_mask_return_claimed ( cpu_set_t *mask )
{
   int i;
   int returned = 0;
   cpu_set_t slaves_mask;

   CPU_XOR( &slaves_mask, mask, &default_mask );
   // Remove extra-cpus (slaves_mask) that have been removed from given_cpus
   for ( i=0; i<mu_get_system_size(); i++ ) {
      if ( CPU_ISSET( i, &slaves_mask ) && !CPU_ISSET( i, &(shdata->given_cpus) ) ) {
         CPU_CLR( i, mask );
         returned++;
      }
   }

   if ( returned > 0 ) {
      debug_shmem ( "Giving back %d Threads\n", returned );
   }

   return returned;
}

int shmem_lewi_mask_collect_mask ( cpu_set_t *mask, int max_resources )
{
   int i;
   int collected = 0;
   int size = CPU_COUNT( &(shdata->avail_cpus) );

   if ( size > 0 && max_resources > 0) {

      cpu_set_t candidates_mask;
      cpu_set_t affinity_mask;
      mu_get_affinity_mask( &affinity_mask, mask, MU_ANY_BIT );

      shmem_lock();
      {
         /* First Step: Retrieve affine cpus */
         CPU_AND( &candidates_mask, &affinity_mask, &(shdata->avail_cpus) );
         for ( i=0; i<mu_get_system_size() && max_resources>0; i++ ) {
            if ( CPU_ISSET( i, &candidates_mask ) ) {
               CPU_CLR( i, &(shdata->avail_cpus) );
               CPU_SET( i, mask );
               max_resources--;
               collected++;
            }
         }
         debug_shmem ( "Getting %d affine Threads (%s)\n", collected, mu_to_str(mask) );

         /* Second Step: Retrieve non-affine cpus, if needed */
         if ( size-collected > 0 && max_resources > 0 ) {

            if ( _priorize_locality ) {
               mu_get_affinity_mask( &affinity_mask, &(shdata->given_cpus), MU_ALL_BITS );
               CPU_AND( &candidates_mask, &affinity_mask, &(shdata->avail_cpus) );
            }
            else {
               memcpy( &candidates_mask, &(shdata->avail_cpus), sizeof(cpu_set_t) );
            }

            for ( i=0; i<mu_get_system_size() && max_resources>0; i++ ) {
               if ( CPU_ISSET( i, &candidates_mask ) ) {
                  CPU_CLR( i, &(shdata->avail_cpus) );
                  CPU_SET( i, mask );
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
      debug_shmem ( "Collecting mask %s\n", mu_to_str(mask) );
      add_event( IDLE_CPUS_EVENT, size - collected );
   }
   return collected;
}
