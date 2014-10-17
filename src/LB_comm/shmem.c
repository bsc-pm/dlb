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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>      /* For mode constants */
#include <fcntl.h>         /* For O_* constants */
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#ifndef _POSIX_THREAD_PROCESS_SHARED
#error This system does not support process shared mutex
#endif

#include "support/debug.h"
#include "support/utils.h"
#include "support/tracing.h"

#define SHM_NAME_LENGTH 32

static int fd;
static void* addr;
static size_t length;
static sem_t *semaphore;
static pthread_mutex_t *shmem_mutex = NULL;
static char shm_filename[SHM_NAME_LENGTH];
static char sem_filename[SHM_NAME_LENGTH];

void shmem_init( void **shdata, size_t sm_size )
{
   debug_shmem ( "Shared Memory Init: pid(%d)\n", getpid() );

   key_t key = getuid();
   char *custom_shm_name;
   parse_env_string( "LB_SHM_NAME", &custom_shm_name );

   if ( custom_shm_name != NULL ) {
      snprintf( sem_filename, sizeof(sem_filename), "/DLB_sem_%s", custom_shm_name );
      snprintf( shm_filename, sizeof(shm_filename), "/DLB_shm_%s", custom_shm_name );
   } else {
      snprintf( sem_filename, sizeof(sem_filename), "/DLB_sem_%d", key );
      snprintf( shm_filename, sizeof(shm_filename), "/DLB_shm_%d", key );
   }

   debug_shmem ( "Start Process Comm - creating shared mem \n" );

   /* Obtain a file descriptor for the shmem */
   fd = shm_open( shm_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
   if ( fd == -1 ) {
      perror( "DLB PANIC: shm_open Process" );
      exit( 1 );
   }

   /* Truncate the regular file to a precise size */
   if ( ftruncate( fd, sm_size ) == -1 ) {
      perror( "DLB PANIC: ftruncate Process" );
      exit( 1 );
   }

   /* Map shared memory object */
   *shdata = mmap( NULL, sm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if ( *shdata == MAP_FAILED ) {
      perror( "DLB PANIC: mmap Process" );
      exit( 1 );
   }

   /* Create Semaphore */
   semaphore = sem_open( sem_filename, O_CREAT, S_IRUSR | S_IWUSR, 1 );
   if ( semaphore == SEM_FAILED ) {
      perror( "DLB_PANIC: Process unable to create/attach to semaphore" );
      // Not sure if we need both
      sem_close( semaphore );
      sem_unlink( sem_filename );
      exit( 1 );
   }

   debug_shmem ( "Start Process Comm - shared mem created\n" );

   /* Save addr and length for the finalize step*/
   addr = *shdata;
   length = sm_size;
}

void shmem_finalize( void )
{
#ifdef IS_BGQ_MACHINE
   // BG/Q have some problems deallocating shmem
   // It will be cleaned after the job completion anyway
   return;
#endif

   if ( shmem_mutex != NULL ) {
      if ( pthread_mutex_destroy( shmem_mutex ) )
         perror ( "DLB ERROR: Shared Memory mutex destroy" );
   }

   if ( sem_close( semaphore ) ) perror( "DLB ERROR: sem_close" );
   if ( munmap( addr, length ) ) perror( "DLB_ERROR: munmap" );
}

void shmem_set_mutex ( pthread_mutex_t *shmutex )
{
   sem_wait( semaphore );
   if ( shmutex != NULL ) {
      pthread_mutexattr_t attr;

      /* Init pthread attributes */
      if ( pthread_mutexattr_init( &attr ) )
         perror( "DLB ERROR: pthread_mutexattr_init" );

      /* Set process-shared attribute */
      if ( pthread_mutexattr_setpshared( &attr, PTHREAD_PROCESS_SHARED ) )
         perror( "DLB ERROR: pthread_mutexattr_setpshared" );

      /* Init pthread mutex */
      if ( pthread_mutex_init( shmutex, &attr ) )
         perror( "DLB ERROR: pthread_mutex_init" );

      /* Destroy pthread attributes */
      if ( pthread_mutexattr_destroy( &attr ) )
         perror( "DLB ERROR: pthread_mutexattr_destroy" );

      shmem_mutex = shmutex;
   }
   sem_post( semaphore );
}

void shmem_lock( void )
{
   if ( shmem_mutex != NULL ) {
      pthread_mutex_lock( shmem_mutex );
   } else {
      sem_wait( semaphore );
   }
}

void shmem_unlock( void )
{
   if ( shmem_mutex != NULL ) {
      pthread_mutex_unlock( shmem_mutex );
   } else {
      sem_post( semaphore );
   }
}

char *get_shm_filename( void )
{
   return shm_filename;
}
