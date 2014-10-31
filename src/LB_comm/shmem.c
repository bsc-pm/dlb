/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
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

#define SHM_NAME_LENGTH 32
#define SHSYNC_MAX_SIZE 64

typedef struct {
    pthread_mutex_t shmem_mutex;
    unsigned short nprocs;
} shsync_t;

static size_t shmem_size;
static char *shmem_addr = NULL;
static shsync_t *shsync = NULL;
static sem_t *semaphore = NULL;
static char shm_filename[SHM_NAME_LENGTH];
static char sem_filename[SHM_NAME_LENGTH];

void shmem_init( void **shdata, size_t shdata_size ) {
    debug_shmem ( "Shared Memory Init: pid(%d)\n", getpid() );

    /* Calculate total shmem size:
     *   shsync_t will be allocated within the first SHSYNC_MAX_SIZE bytes
     *   shdata_t will use the rest
     */
    ensure( sizeof(shsync_t) <= SHSYNC_MAX_SIZE,
            "Sync structure must be %d bytes maximum\n", SHSYNC_MAX_SIZE );
    shmem_size = SHSYNC_MAX_SIZE + shdata_size;

    /* Get /dev/shm/ file names to create */
    key_t key = getuid();
    char *custom_shm_name;
    parse_env_string( "LB_SHM_NAME", &custom_shm_name );

    if ( custom_shm_name != NULL ) {
        snprintf( shm_filename, sizeof(shm_filename), "/DLB_shm_%s", custom_shm_name );
        snprintf( sem_filename, sizeof(sem_filename), "/DLB_sem_%s", custom_shm_name );
    } else {
        snprintf( shm_filename, sizeof(shm_filename), "/DLB_shm_%d", key );
        snprintf( sem_filename, sizeof(sem_filename), "/DLB_sem_%d", key );
    }

    debug_shmem ( "Start Process Comm - creating shared mem \n" );

    /* Obtain a file descriptor for the shmem */
    int fd = shm_open( shm_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
    if ( fd == -1 ) {
        perror( "DLB PANIC: shm_open Process" );
        exit( EXIT_FAILURE );
    }

    /* Truncate the regular file to a precise size */
    if ( ftruncate( fd, shmem_size ) == -1 ) {
        perror( "DLB PANIC: ftruncate Process" );
        exit( EXIT_FAILURE );
    }

    /* Map shared memory object */
    shmem_addr = mmap( NULL, shmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ( shmem_addr == MAP_FAILED ) {
        perror( "DLB PANIC: mmap Process" );
        exit( EXIT_FAILURE );
    }

    /* Create Semaphore(1): Mutex */
    semaphore = sem_open( sem_filename, O_CREAT, S_IRUSR | S_IWUSR, 1 );
    if ( semaphore == SEM_FAILED ) {
        perror( "DLB_PANIC: Process unable to create/attach to semaphore" );
        exit( EXIT_FAILURE );
    }

    debug_shmem ( "Start Process Comm - shared mem created\n" );

    /* Set the address for both structs */
    shsync = (shsync_t*) shmem_addr;
    *shdata = shmem_addr + SHSYNC_MAX_SIZE;

    /* Set up shsync struct: increment reference counter and initialize pthread_mutex */
    sem_wait( semaphore );
    shsync->nprocs++;
    if ( shsync->nprocs == 1 ) {
        pthread_mutexattr_t attr;

        /* Init pthread attributes */
        if ( pthread_mutexattr_init( &attr ) ) {
            perror( "DLB ERROR: pthread_mutexattr_init" );
        }

        /* Set process-shared attribute */
        if ( pthread_mutexattr_setpshared( &attr, PTHREAD_PROCESS_SHARED ) ) {
            perror( "DLB ERROR: pthread_mutexattr_setpshared" );
        }

        /* Init pthread mutex */
        if ( pthread_mutex_init( &(shsync->shmem_mutex), &attr ) ) {
            perror( "DLB ERROR: pthread_mutex_init" );
        }

        /* Destroy pthread attributes */
        if ( pthread_mutexattr_destroy( &attr ) ) {
            perror( "DLB ERROR: pthread_mutexattr_destroy" );
        }
    }
    sem_post( semaphore );
}

void shmem_finalize( void ) {
#ifdef IS_BGQ_MACHINE
    // BG/Q have some problems deallocating shmem
    // It will be cleaned after the job completion anyway
    return;
#endif

    /* Decrement reference counter */
    sem_wait( semaphore );
    int nprocs = --(shsync->nprocs);
    sem_post( semaphore );

    /* Only the last process destroys the pthread_mutex */
    if ( nprocs == 0 ) {
        if ( pthread_mutex_destroy( &(shsync->shmem_mutex) ) ) {
            perror ( "DLB ERROR: Shared Memory mutex destroy" );
        }
    }

    /* All processes must close semaphores and unmap shmem */
    if ( sem_close( semaphore ) ) { perror( "DLB ERROR: sem_close" ); }
    if ( munmap( shmem_addr, shmem_size ) ) { perror( "DLB_ERROR: munmap" ); }

    /* Only the last process unlinks semaphores and shmem */
    if ( nprocs == 0 ) {
        if ( sem_unlink( sem_filename ) ) { perror( "DLB_ERROR: sem_unlink" ); }
        if ( shm_unlink( shm_filename ) ) { perror( "DLB ERROR: shm_unlink" ); }
    }
}

void shmem_lock( void ) {
    if ( shsync != NULL ) {
        pthread_mutex_lock( &(shsync->shmem_mutex) );
    } else {
        sem_wait( semaphore );
    }
}

void shmem_unlock( void ) {
    if ( shsync != NULL ) {
        pthread_mutex_unlock( &(shsync->shmem_mutex) );
    } else {
        sem_post( semaphore );
    }
}

char *get_shm_filename( void ) {
    return shm_filename;
}
