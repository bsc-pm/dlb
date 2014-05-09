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
#include "config.h"
#endif

#include <unistd.h>
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

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

static int fd;
static void* addr;
static size_t length;
static sem_t *semaphore;
static pthread_mutex_t *shmem_mutex = NULL;
static char shm_filename[32];    /* 32 chars should be enough to store /DLB_xxx_$PID\0 */
static char sem_filename[32];    /* even in systems where PID_MAX has been increased   */

void shmem_init( void **shdata, size_t sm_size )
{
   debug_shmem ( "Shared Memory Init: pid(%d)\n", getpid() );

   key_t key;

   if ( _process_id == 0 ) {

      debug_shmem ( "Start Master Comm - creating shared mem \n" );

      key = getpid();

      /* Create Semaphore */
      sprintf( sem_filename, "/DLB_sem_%d", key );
      semaphore = sem_open( sem_filename, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1 );
      if ( semaphore == SEM_FAILED ) {
         perror( "DLB_PANIC: Master unable to create semaphore" );
         sem_unlink( sem_filename );
         exit( 1 );
      }

      /* Obtain a file descriptor for the shmem */
      sprintf( shm_filename, "/DLB_shm_%d", key );
      fd = shm_open( shm_filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR );
      if ( fd == -1 ) {
         perror( "DLB PANIC: shm_open Master" );
         exit( 1 );
      }

      /* Truncate the regular file to a precise size */
      if ( ftruncate( fd, sm_size ) == -1 ) {
         perror( "DLB PANIC: ftruncate Master" );
         exit( 1 );
      }

      /* Map shared memory object */
      *shdata = mmap( NULL, sm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if ( *shdata == MAP_FAILED ) {
         perror( "DLB PANIC: mmap Master" );
         exit( 1 );
      }

#ifdef HAVE_MPI
      PMPI_Bcast ( &key, 1, MPI_INTEGER, 0, _mpi_comm_node );
#endif

      debug_shmem ( "Start Master Comm - shared mem created\n" );

   } else {
      debug_shmem ( "Slave Comm - associating to shared mem\n" );

#ifdef HAVE_MPI
      PMPI_Bcast ( &key, 1, MPI_INTEGER, 0, _mpi_comm_node );
#else
      key = 0;
#endif

      /* Open Semaphore */
      sprintf( sem_filename, "/DLB_sem_%d", key );
      semaphore = sem_open( sem_filename, 0, S_IRUSR | S_IWUSR, 0 );
      if ( semaphore == SEM_FAILED ) {
         perror( "DLB PANIC: Reader unable to open semaphore" );
         sem_close( semaphore );
         exit( 1 );
      }

      /* Obtain a file descriptor for the shmem */
      sprintf( shm_filename, "/DLB_shm_%d", key );
      do {
         fd = shm_open( shm_filename, O_RDWR, S_IRUSR | S_IWUSR );
      } while ( fd < 0 && errno == ENOENT );

      if ( fd < 0 ) {
         perror( "DLB PANIC: shm_open Slave" );
         exit( 1 );
      }

      debug_shmem ( "Slave Comm - associated to shared mem\n" );

      /* Map shared memory object */
      *shdata = mmap( NULL, sm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if ( *shdata == MAP_FAILED ) {
         perror( "DLB PANIC: mmap Slave" );
         exit( 1 );
      }
   }

   /* Store addr and length for the finalize step*/
   addr = *shdata;
   length = sm_size;
}

void shmem_finalize( void )
{
   if ( shmem_mutex != NULL ) {
      if ( pthread_mutex_destroy( shmem_mutex ) )
         perror ( "DLB ERROR: Shared Memory mutex destroy" );
   }

   sem_close( semaphore );
   if ( _process_id == 0 ) {
      sem_unlink(sem_filename);
   }

   if ( munmap( addr, length ) )
      perror( "DLB ERROR: munmap" );

   if ( _process_id == 0 ) {
      if ( shm_unlink( shm_filename ) )
         perror( "DLB ERROR: shm_unlink" );
   }
}

void shmem_set_mutex ( pthread_mutex_t *shmutex )
{
   if ( shmutex != NULL ) {
      pthread_mutexattr_t attr;

      /* Init pthread attributes */
      if ( pthread_mutexattr_init( &attr ) )
         perror( "DLB ERROR: " );

      /* Set process-shared attribute */
      if ( pthread_mutexattr_setpshared( &attr, PTHREAD_PROCESS_SHARED ) )
         perror( "DLB ERROR: " );

      /* Init pthread mutex */
      if ( pthread_mutex_init( shmutex, &attr ) )
         perror( "DLB ERROR: " );

      /* Destroy pthread attributes */
      if ( pthread_mutexattr_destroy( &attr ) )
         perror( "DLB ERROR: " );

      shmem_mutex = shmutex;
   }
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
