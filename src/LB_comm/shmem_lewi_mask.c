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
} shdata_t;

static shdata_t *shdata;
static cpu_set_t default_mask;


const char* mask_to_str ( cpu_set_t *cpu_set )
{
   int i;
   static char str[CPU_SETSIZE*2 + 4];
   strcpy( str, "[ " );
   for ( i=0; i<12; i++ ) {
      if ( CPU_ISSET(i, cpu_set) ) strcat( str, "1 " );
      else strcat( str,"0 " );
   }
   strcat( str, "]\0" );
   return str;
}

void shmem_lewi_mask_init( cpu_set_t *cpu_set )
{
   shmem_init( &shdata, sizeof(shdata_t) );
   add_event( IDLE_CPUS_EVENT, 0 );
   if ( _process_id == 0 ) {
      CPU_ZERO( &(shdata->free_cpus) );
   }

   memcpy( &default_mask, cpu_set, sizeof(cpu_set_t) );
   debug_shmem( "Default Mask: %s\n", mask_to_str(&default_mask));
}

void shmem_lewi_mask_finalize( void )
{
   while ( CPU_COUNT( &(shdata->free_cpus) ) != 0 );
   shmem_finalize();
}

void shmem_lewi_mask_add_mask( cpu_set_t *cpu_set )
{
   int size = CPU_COUNT( cpu_set );
   int post_size;

   shmem_lock();
   {
      CPU_OR( &(shdata->free_cpus), &(shdata->free_cpus), cpu_set );
      post_size = CPU_COUNT( &(shdata->free_cpus) );
      debug_shmem ( "Increasing %d Idle Threads (%d)\n", size, post_size );
      debug_shmem ( "Free mask: %s\n", mask_to_str(&(shdata->free_cpus)) );
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );
}

cpu_set_t* shmem_lewi_mask_recover_defmask( void )
{
   int prev_size, post_size, i;

   shmem_lock();
   {
      prev_size = CPU_COUNT( &(shdata->free_cpus) );
      for ( i = 0; i < CPU_SETSIZE; i++ ) {
         if ( CPU_ISSET(i, &default_mask) ) {
            CPU_CLR( i, &(shdata->free_cpus) );
         }
      }
      post_size = CPU_COUNT( &(shdata->free_cpus) );
      debug_shmem ( "Decreasing %d Idle Threads (%d)\n", prev_size - post_size, post_size );
      debug_shmem ( "Free mask: %s\n", mask_to_str(&(shdata->free_cpus)) ) ;
   }
   shmem_unlock();

   add_event( IDLE_CPUS_EVENT, post_size );

   return &default_mask;
}

int shmem_lewi_mask_collect_mask ( cpu_set_t *cpu_set, int max_resources )
{
   int i;
   int size = 0;
   int collected = 0;

   shmem_lock();
   {
      size = CPU_COUNT( &(shdata->free_cpus) );
      if ( size > 0 ) {
         for ( i=0; i<CPU_SETSIZE && max_resources>0; i++ ) {
            if ( CPU_ISSET( i, &(shdata->free_cpus) ) ) {
               CPU_CLR( i, &(shdata->free_cpus) );
               CPU_SET( i, cpu_set );
               max_resources--;
               collected++;
            }
         }
      }
   }
   shmem_unlock();

   if ( collected > 0 ) {
      debug_shmem ( "Clearing %d Idle Threads (%d)\n", collected,  size - collected );
      debug_shmem ( "Collecting mask %s, (size=%d)\n", mask_to_str( cpu_set ), collected );
      add_event( IDLE_CPUS_EVENT, size - collected );
   }

   return collected;
}
