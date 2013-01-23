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
#include <stdlib.h>
#include <strings.h>

#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_lewi_mask.h"
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

typedef enum {
   ONE_CPU, // MPI not set to blocking, leave a cpu while in a MPI blockin call
   BLOCK,   // MPI set to blocking mode
} blocking_mode_t;

static int nthreads;
static bool lock;
static blocking_mode_t blocking_mode;

static void set_threads( int threads );
static void parse_blocking_mode( void );


/******* Main Functions - LeWI Balancing Policy ********/

void lewi_mask_init( void )
{
   debug_config ( "LeWI Balancing Init\n" );

   nthreads = _default_nthreads;
   lock = false;

   // Setting blocking mode
   parse_blocking_mode();

   //Initialize shared memory
   cpu_set_t cpu_set;
   get_mask( &cpu_set );
   shmem_lewi_mask_init( &cpu_set );
}

void lewi_mask_finish( void )
{
   if ( _process_id == 0 ) shmem_lewi_mask_finalize();
}

void lewi_mask_init_iteration( void ) {}

void lewi_mask_finish_iteration( double cpu_secs, double MPI_secs ) {}

void lewi_mask_into_communication( void ) {}

void lewi_mask_out_of_communication( void ) {}

/* Into Blocking Call - Lend the maximum number of threads */
void lewi_mask_into_blocking_call( double cpu_secs, double MPI_secs )
{
   cpu_set_t cpu_set;
   get_mask( &cpu_set );

   add_event ( MPI_BLOCKED_EVENT, 1);
   lock = true;
   if ( blocking_mode == ONE_CPU ) {
      // Remove current cpu from the set
      cpu_set_t my_cpu;
      sched_getaffinity( 0, sizeof(cpu_set_t), &my_cpu);
      CPU_XOR( &cpu_set, &cpu_set, &my_cpu );

      debug_lend ( "LENDING %d threads\n", nthreads-1 );
      set_mask( &my_cpu );
      set_threads( 1 );
   }
   else if ( blocking_mode == BLOCK ) {
      debug_lend ( "LENDING %d threads\n", nthreads );
      set_threads( 0 );
   }
   shmem_lewi_mask_add_mask( &cpu_set );
}

/* Out of Blocking Call - Recover the default number of threads */
void lewi_mask_out_of_blocking_call( void )
{
   set_mask( shmem_lewi_mask_recover_defmask() );
   set_threads( _default_nthreads );

   // Because of 1CPU...
   debug_lend ( "RECOVERING %d threads\n", _default_nthreads - 1 );
   //debug_lend ( "RECOVERING %d threads\n", _default_nthreads - nthreads );

   add_event ( MPI_BLOCKED_EVENT, 0);
   lock = false;
}

/* Update Resources - Try to acquire foreign threads */
void lewi_mask_update_resources( void )
{
   if ( !lock ) {
      cpu_set_t free_cpus;
      int new_threads = shmem_lewi_mask_collect_mask( &free_cpus );

      if ( new_threads > 0 ) {
         int threads = __sync_add_and_fetch( &nthreads, new_threads ) ;
         add_mask( &free_cpus );
         // already updated by mask
         //update_threads( threads );
         debug_lend ( "ACQUIRING %d threads for a total of %d\n", new_threads, threads );
      }
   }
}

/******* Auxiliar Functions - LeWI Balancing Policy ********/

static void set_threads( int new_threads )
{
   int old_threads =  __sync_lock_test_and_set ( &nthreads, new_threads );
   if ( new_threads != old_threads ) {
      // already updated by mask
      //update_threads( new_threads );
   }
}

static void parse_blocking_mode( void )
{
   char* blocking = getenv( "LB_LEND_MODE" );
   blocking_mode = ONE_CPU;

   if ( blocking != NULL ) {
      if ( strcasecmp( blocking, "1CPU" ) == 0 ) {
         blocking_mode = ONE_CPU;
         debug_basic_info0 ( "LEND mode set to 1CPU. I will leave a cpu per MPI process when in an MPI call\n" );
      }
      else if ( strcasecmp( blocking, "BLOCK" ) == 0 ) {
         blocking_mode = BLOCK;
         debug_basic_info0 ( "LEND mode set to BLOCKING. I will lend all the resources when in an MPI call\n" );
      }
   }
}
