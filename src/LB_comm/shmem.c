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

#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

static int shmid;
static sem_t *mutex;
static const char SEM_NAME[]= "DLB_mutex";

void shmem_init( void **shdata, size_t sm_size )
{
   debug_shmem ( "Shared Memory Init: pid(%d)\n", getpid() );

   key_t key;

   if ( _process_id == 0 ) {

      debug_shmem ( "Start Master Comm - creating shared mem \n" );

      key = getpid();

      sem_unlink( SEM_NAME );
      mutex = sem_open( SEM_NAME, O_CREAT | O_EXCL, 0666, 1 );
      if ( mutex == SEM_FAILED ) {
         perror( "unable to create semaphore" );
         sem_unlink( SEM_NAME );
         exit( 1 );
      }

      if ( ( shmid = shmget( key, sm_size, IPC_EXCL | IPC_CREAT | 0666 ) ) < 0 ) {
         perror( "DLB PANIC: shmget Master" );
         exit( 1 );
      }

      if ( ( *shdata = shmat( shmid, NULL, 0 ) ) == ( void * ) -1 ) {
         perror( "DLB PANIC: shmat Master" );
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

      mutex = sem_open( SEM_NAME, 0, 0666, 0 );
      if ( mutex == SEM_FAILED ) {
         perror( "reader:unable to execute semaphore" );
         sem_close( mutex );
         exit( 1 );
      }

      do {
         shmid = shmget( key, sm_size, 0666 );
      } while ( shmid<0 && errno==ENOENT );

      if ( shmid < 0 ) {
         perror( "shmget slave" );
         exit( 1 );
      }

      debug_shmem ( "Slave Comm - associated to shared mem\n" );

      if ( ( *shdata = shmat( shmid, NULL, 0 ) ) == ( void * ) -1 ) {
         perror( "shmat slave" );
         exit( 1 );
      }
   }
}

void shmem_finalize( void )
{
   sem_close(mutex);
   if ( _process_id == 0 ) {
      sem_unlink(SEM_NAME);
   }

   if ( shmctl( shmid, IPC_RMID, NULL ) < 0 )
      perror( "DLB ERROR: Removing Shared Memory" );
}

void shmem_lock( void )
{
   sem_wait( mutex );
}

void shmem_unlock( void )
{
   sem_post( mutex );
}
