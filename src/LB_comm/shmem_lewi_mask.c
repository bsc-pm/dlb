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

typedef struct {
   /*
    * When a process gives their cpus, they are added to both sets.
    * When a process recovers their cpus, they are removed from both sets.
    * When a process collects cpus, they are removed only from avail_cpus,
    *    and if it owns cpus not present in given_cpus or its own cpus, it must give them back.
    */
   cpu_set_t given_cpus; // set of given cpus. Only modified by its own process.
   cpu_set_t avail_cpus; // set of available cpus at the moment
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;

void shmem_lewi_mask_init( cpu_set_t *cpu_set )
{
   shmem_init( &shdata, sizeof(shdata_t) );
   add_event( IDLE_CPUS_EVENT, 0 );
   if ( _process_id == 0 ) {
      CPU_ZERO( &(shdata->given_cpus) );
      CPU_ZERO( &(shdata->avail_cpus) );
   }

   memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
   debug_shmem( "Default Mask: %s\n", mask_to_str(&default_mask));
}

void shmem_lewi_mask_finalize( void )
{
   while ( CPU_COUNT( &(shdata->given_cpus) ) != 0 );
   shmem_finalize();
}

void shmem_lewi_mask_add_mask( cpu_set_t *cpu_set )
{
   int size = CPU_COUNT( cpu_set );
   int post_size;

   shmem_lock();
   {
      CPU_OR( &(shdata->given_cpus), &(shdata->given_cpus), cpu_set );
      CPU_OR( &(shdata->avail_cpus), &(shdata->avail_cpus), cpu_set );
      post_size = CPU_COUNT( &(shdata->avail_cpus) );
      debug_shmem ( "Increasing %d Idle Threads (%d)\n", size, post_size );
      debug_shmem ( "Available mask: %s\n", mask_to_str(&(shdata->avail_cpus)) );
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );
}

cpu_set_t* shmem_lewi_mask_recover_defmask( void )
{
   int prev_size, post_size, i;

   shmem_lock();
   {
      prev_size = CPU_COUNT( &(shdata->avail_cpus) );
      for ( i = 0; i < CPU_SETSIZE; i++ ) {
         if ( CPU_ISSET(i, &default_mask) ) {
            CPU_CLR( i, &(shdata->given_cpus) );
            CPU_CLR( i, &(shdata->avail_cpus) );
         }
      }
      post_size = CPU_COUNT( &(shdata->avail_cpus) );
      debug_shmem ( "Decreasing %d Idle Threads (%d)\n", prev_size - post_size, post_size );
      debug_shmem ( "Available mask: %s\n", mask_to_str(&(shdata->avail_cpus)) ) ;
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );

   return &default_mask;
}

bool shmem_lewi_mask_collect_mask ( cpu_set_t *mask, int max_resources, int *new_threads )
{
   int size = 0;
   int returned = 0;
   int collected = 0;
   bool dirty = false;
   cpu_set_t slaves_mask;
   CPU_ZERO( &slaves_mask );
   CPU_XOR( &slaves_mask, mask, &default_mask );

   shmem_lock();
   {
      size = CPU_COUNT( &(shdata->avail_cpus) );

      int i;
      for ( i=0; i<CPU_SETSIZE && max_resources>0; i++ ) {
         if ( CPU_ISSET( i, &(shdata->avail_cpus) ) ) {
            CPU_CLR( i, &(shdata->avail_cpus) );
            CPU_SET( i, mask );
            max_resources--;
            collected++;
            dirty = true;
         }
         else {
            // If the cpu is a slave, but it's been removed from given_cpus, then remove it
            if ( CPU_ISSET( i, &slaves_mask ) && !CPU_ISSET( i, &(shdata->given_cpus) ) ) {
               CPU_CLR( i, mask );
               max_resources++;
               returned++;
               dirty = true;
            }
         }
      }
   }
   shmem_unlock();

   if ( dirty ) {
      debug_shmem ( "Clearing %d Idle Threads (%d)\n", collected,  size - collected );
      debug_shmem ( "Collecting mask %s, (size=%d)\n", mask_to_str(mask) );
      add_event( IDLE_CPUS_EVENT, size - collected );
      *new_threads = collected - returned;
   }

   return dirty;
}
