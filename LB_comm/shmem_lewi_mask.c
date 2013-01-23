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

typedef struct {
   cpu_set_t free_cpus;
   int ready_procs;  // It works as a lock -> Every process returning from an MPI Barrier will increse this valuy by 1.
                     // No process will be able to collect threads from the Pool until all processes have set this value.
                     // The last process will have to set it to 0
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;

static void print_mask( cpu_set_t *cpu_set );

void shmem_lewi_mask_init( cpu_set_t *cpu_set )
{
   shmem_init( &shdata, sizeof(shdata_t) );
   add_event( IDLE_CPUS_EVENT, 0 );
   if ( _process_id == 0 ) {
      CPU_ZERO( &(shdata->free_cpus) );
      print_mask( &(shdata->free_cpus) );
   }

   memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
}

void shmem_lewi_mask_finalize( void )
{
   shmem_finalize();
}

void shmem_lewi_mask_add_mask( cpu_set_t *cpu_set )
{
   int size = CPU_COUNT( cpu_set );

   shmem_lock();
   {
      CPU_OR( &(shdata->free_cpus), &(shdata->free_cpus), cpu_set );
      print_mask( &(shdata->free_cpus) );
      debug_shmem ( "Increasing %d Idle Threads (%d)\n", size, CPU_COUNT( &(shdata->free_cpus) ) );
      add_event( IDLE_CPUS_EVENT, size );
   }
   shmem_unlock();
}

cpu_set_t* shmem_lewi_mask_recover_defmask( void )
{
   int size, gone, i;
   __sync_add_and_fetch ( &( shdata->ready_procs ), 1 );
   __sync_val_compare_and_swap( &( shdata->ready_procs ), _mpis_per_node, 0 );

   shmem_lock();
   {
      for ( i = 0, gone = 0; i < CPU_SETSIZE; i++ ) {
         if ( CPU_ISSET(i, &default_mask) ) {
            CPU_CLR( i, &(shdata->free_cpus) );
            gone++;
         }
      }
      size = CPU_COUNT( &(shdata->free_cpus) );
      print_mask( &(shdata->free_cpus) );
      add_event( IDLE_CPUS_EVENT, size );
      debug_shmem ( "Decreasing %d Idle Threads (%d)\n", gone-1, size );
   }
   shmem_unlock();

   return &default_mask;
}

int shmem_lewi_mask_collect_mask ( cpu_set_t *cpu_set )
{
   int size = 0;

   shmem_lock();
   {
      size = CPU_COUNT( &(shdata->free_cpus) );
      print_mask( &(shdata->free_cpus) );
      if ( size > 0 && shdata->ready_procs == 0) {
         memcpy( cpu_set, &(shdata->free_cpus), sizeof(cpu_set_t) );
         CPU_ZERO( &(shdata->free_cpus) );
         debug_shmem ( "Clearing Idle Threads (0)\n" );
      }
   }
   shmem_unlock();

   return size;
}

static void print_mask ( cpu_set_t *cpu_set )
{
   int i;
   char str[CPU_SETSIZE*2 + 8];
   strcpy( str, "[ " );
   for ( i=0; i<12; i++ ) {
      if ( CPU_ISSET(i, cpu_set) ) strcat( str, "1 " );
      else strcat( str,"0 " );
   }
   strcat( str, "]\n" );
   debug_shmem( str );
}
