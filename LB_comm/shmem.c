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

#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <mpi.h>

#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

static int shmid;
static int semid;
static struct sembuf lock;
static struct sembuf unlock;

void shmem_init( void **shdata, size_t sm_size )
{
   debug_shmem ( "Shared Memory Init: pid(%d)\n", getpid() );

   key_t key;

   MPI_Comm comm_node;
   MPI_Comm_split ( MPI_COMM_WORLD, _node_id, 0, &comm_node );

   lock.sem_num = 0;
   lock.sem_op = -1;
   lock.sem_flg = 0;

   unlock.sem_num = 0;
   unlock.sem_op = 1;
   unlock.sem_flg = 0;

   if ( _process_id == 0 ) {

      debug_shmem ( "Start Master Comm - creating shared mem \n" );

      key = getpid();

      short sem_init = 1;
      if ( ( semid = semget( key, 1, IPC_EXCL | IPC_CREAT | 0666 ) ) < 0 ) {
         perror( "DLB PANIC: semget Master" );
         exit( 1 );
      }

      if ( semctl( semid, 0, SETALL, &sem_init ) < 0 ) {
         perror( "DLB PANIC: semctl Master" );
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

      PMPI_Bcast ( &key, 1, MPI_INTEGER, 0, comm_node );

      debug_shmem ( "Start Master Comm - shared mem created\n" );

   } else {
      debug_shmem ( "Slave Comm - associating to shared mem\n" );

      PMPI_Bcast ( &key, 1, MPI_INTEGER, 0, comm_node );

      semid = semget( key, 1, 0666 );

      do {
         shmid = shmget( key, sm_size, 0666 );
      } while ( shmid<0 && errno==ENOENT );

      if ( shmid < 0 || semid < 0 ) {
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
   if ( semctl( semid, 1, IPC_RMID ) < 0 )
      perror( "DLB ERROR: Removing Semaphore" );

   if ( shmctl( shmid, IPC_RMID, NULL ) < 0 )
      perror( "DLB ERROR: Removing Shared Memory" );
}

void shmem_lock( void )
{
   if ( semop( semid, &lock, 1 ) < 0 ) {
      perror("shmctl: LOCK failed");
      exit( 1 );
   }
   add_event( 800050, 1);
}

void shmem_unlock( void )
{
   if ( semop( semid, &unlock, 1 ) < 0 ) {
      perror("shmctl: UNLOCK failed");
      exit( 1 );
   }
   add_event( 800050, 0);
}
