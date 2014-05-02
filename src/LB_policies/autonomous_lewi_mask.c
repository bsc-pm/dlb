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
#include "support/utils.h"
#include "support/mask_utils.h"

#include <pthread.h>
#include <assert.h>


static int nthreads;
static pthread_mutex_t mutex;
static int enabled=0;
/******* Main Functions - LeWI Mask Balancing Policy ********/

void auto_lewi_mask_Init( void )
{
   debug_config ( "LeWI Mask Balancing Init\n" );

   nthreads = _default_nthreads;

   //Initialize shared memory
   cpu_set_t default_mask;
   get_mask( &default_mask );
   shmem_lewi_mask_init( &default_mask );

   if ( _aggressive_init ) {
      cpu_set_t mask;
      CPU_ZERO( &mask );

      int i;
      for ( i = 0; i < mu_get_system_size(); i++ ) {
         CPU_SET( i, &mask );
      }
      set_mask( &mask );
      set_mask( &default_mask );
   }

   pthread_mutex_init(&mutex, NULL);
   enabled=1;
}

void auto_lewi_mask_Finish( void )
{
   pthread_mutex_lock (&mutex);
   set_mask( shmem_lewi_mask_recover_defmask() );
   shmem_lewi_mask_finalize();
   enabled=0;
   pthread_mutex_unlock (&mutex);
   pthread_mutex_destroy(&mutex);
}

void auto_lewi_mask_IntoCommunication( void ) {}

void auto_lewi_mask_OutOfCommunication( void ) {}

/* Into Blocking Call - Lend the maximum number of threads */
void auto_lewi_mask_IntoBlockingCall(int is_iter, int blocking_mode)
{
/*
   cpu_set_t cpu;
   CPU_ZERO( &cpu );
   sched_getaffinity( 0, sizeof(cpu_set_t), &cpu);

   cpu_set_t mask;
   CPU_ZERO( &mask );

   pthread_mutex_lock (&mutex);
   get_mask( &mask );

   if ( blocking_mode == ONE_CPU ) {
      // Remove current cpu from the mask
      CPU_XOR( &mask, &mask, &cpu );
      debug_lend ( "LENDING %d threads\n", nthreads-1 );
      nthreads = 1;
   }
   else if ( _blocking_mode == BLOCK ) {
      debug_lend ( "LENDING %d threads\n", nthreads );
      nthreads = 0;
   }

   set_mask( &cpu );
   shmem_lewi_mask_add_mask( &mask );
   pthread_mutex_unlock (&mutex);

   add_event( THREADS_USED_EVENT, nthreads );
*/
}

/* Out of Blocking Call - Recover the default number of threads */
void auto_lewi_mask_OutOfBlockingCall(int is_iter)
{
  /* debug_lend ( "RECOVERING %d threads\n", _default_nthreads - nthreads );
   pthread_mutex_lock (&mutex);

   set_mask( shmem_lewi_mask_recover_defmask() );
   nthreads = _default_nthreads;

   add_event( THREADS_USED_EVENT, nthreads );
   pthread_mutex_unlock (&mutex);
*/
}

/* Update Resources - Try to acquire foreign threads */
void auto_lewi_mask_UpdateResources( int max_resources )
{
   cpu_set_t mask;
   CPU_ZERO( &mask );

   pthread_mutex_lock (&mutex);
   if (enabled){
      get_mask( &mask );


      int collected = shmem_lewi_mask_collect_mask( &mask, max_resources );

      if ( collected > 0 ) {
         nthreads += collected;
         set_mask( &mask );
         debug_lend ( "ACQUIRING %d threads for a total of %d\n", collected, nthreads );
         add_event( THREADS_USED_EVENT, nthreads );
      }
      assert(nthreads==CPU_COUNT(&mask));
      debug_shmem ( "My mask %s\n", mu_to_str(&mask));
   }
   pthread_mutex_unlock (&mutex);
}

/* Return Claimed CPUs - Return foreign threads that have been claimed by its owner */
void auto_lewi_mask_ReturnClaimedCpus( void )
{
   cpu_set_t mask;
   CPU_ZERO( &mask );

   pthread_mutex_lock (&mutex);
   if (enabled){
      get_mask( &mask );


      int returned = shmem_lewi_mask_return_claimed( &mask );

      if ( returned > 0 ) {
         nthreads -= returned;
         // If final nthreads is 0, get at least one cpu from the default mask
         if ( nthreads == 0 ) {
            shmem_lewi_mask_recover_some_defcpus( &mask, 1 );
            nthreads = 1;
         }
         set_mask( &mask );
         debug_lend ( "RETURNING %d threads for a total of %d\n", returned, nthreads );
         add_event( THREADS_USED_EVENT, nthreads );
         assert(nthreads==CPU_COUNT(&mask));
      }
      debug_shmem ( "My mask %s\n", mu_to_str(&mask));
   }
   pthread_mutex_unlock (&mutex);
}

int auto_lewi_mask_ReturnCpuIfClaimed( int cpu )
{
   int returned=0;

   if (checkCPUIsClaimed( cpu )){
      add_event(RUNTIME_EVENT, EVENT_RETURN_CPU);
      cpu_set_t release_mask;
      CPU_ZERO( &release_mask );
      CPU_SET(cpu, &release_mask);

      cpu_set_t mask;
      CPU_ZERO( &mask );


      pthread_mutex_lock (&mutex);
      if (enabled){
         get_mask( &mask );

         if ( CPU_ISSET( cpu, &mask)){
            returned = shmem_lewi_mask_return_claimed( &release_mask );

            if ( returned > 0 ) {
               nthreads -= returned;
               CPU_CLR(cpu, &mask);

               // If final nthreads is 0, get at least one cpu from the default mask
               if ( nthreads == 0 ) {
                  shmem_lewi_mask_recover_some_defcpus( &mask, 1 );
                  nthreads = 1;
               }
               set_mask( &mask );
               debug_lend ( "RETURNING MY CPU %d returned threads for a total of %d\n", returned, nthreads );
               add_event( THREADS_USED_EVENT, nthreads );
               assert(nthreads==CPU_COUNT(&mask));
            }
            debug_shmem ( "My mask %s\n", mu_to_str(&mask));
         }
      }
      pthread_mutex_unlock (&mutex);
      add_event(RUNTIME_EVENT, 0);
   }
   return returned;
}

int auto_lewi_mask_ReleaseCpu( int cpu )
{
   add_event(RUNTIME_EVENT, EVENT_RELEASE_CPU);
   cpu_set_t my_cpu;
   sched_getaffinity( 0, sizeof(cpu_set_t), &my_cpu);
   assert(CPU_ISSET(cpu, &my_cpu));

   int released=0;
   debug_lend ( "Lending cpu:  %d \n", cpu );
  // add_event(1000, cpu);

//debug_basic_info ( "I release cpu %d\n", cpu);

   cpu_set_t debug_mask;
   cpu_set_t release_mask;
   CPU_ZERO( &release_mask );
   CPU_SET(cpu, &release_mask);

//debug_basic_info ( "release mask %s\n", mu_to_str(&release_mask));

   cpu_set_t current_mask;
   CPU_ZERO( &current_mask );

   pthread_mutex_lock (&mutex);
   if (enabled){
      get_mask( &current_mask );
      get_mask(&debug_mask);

      if (CPU_ISSET(cpu, &current_mask) && nthreads>1){
         //debug_basic_info ( "current mask %s\n", mu_to_str(&current_mask));

         CPU_XOR( &current_mask, &current_mask, &release_mask );

         //debug_basic_info ( "new mask %s\n", mu_to_str(&current_mask));


         set_mask( &current_mask );
         nthreads--;
         shmem_lewi_mask_add_mask( &release_mask );
         add_event( THREADS_USED_EVENT, nthreads );
         released=1;
      }
      /*else
        assert(CPU_ISSET(cpu, &current_mask));*/

      assert(nthreads==CPU_COUNT(&current_mask));
      debug_shmem ( "My mask %s\n", mu_to_str(&current_mask));
   }
   pthread_mutex_unlock (&mutex);


   add_event(RUNTIME_EVENT, 0);
//   add_event(1000, 0);
   return released;
}

void auto_lewi_mask_ClaimCpus(int cpus)
{
   if (nthreads<_default_nthreads){
      //Do not get more cpus than the default ones
      pthread_mutex_lock (&mutex);
      if (enabled){

         if ((cpus+nthreads)>_default_nthreads) cpus=_default_nthreads-nthreads;

         debug_lend ( "Claiming  %d cpus\n", cpus );
         cpu_set_t debug_mask;
         cpu_set_t current_mask;
         CPU_ZERO( &current_mask );

         get_mask( &current_mask );
         get_mask(&debug_mask);


         shmem_lewi_mask_recover_some_defcpus( &current_mask, cpus ); 
         set_mask( &current_mask);
         nthreads += cpus;
         assert(nthreads==CPU_COUNT(&current_mask));

         add_event( THREADS_USED_EVENT, nthreads );
         debug_shmem ( "My mask %s\n", mu_to_str(&current_mask));
      }
      pthread_mutex_unlock (&mutex);
   }
}

int auto_lewi_mask_CheckCpuAvailability ( int cpu ){
   int available=0;
   available=checkCpuBorrowed(cpu);
   return available;
}
