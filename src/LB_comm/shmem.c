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

#include "LB_comm/shmem.h"
#include "support/debug.h"
#include "support/utils.h"

#define SHSYNC_MAX_SIZE 64

shmem_handler_t* shmem_init( void **shdata, size_t shdata_size, const char* shmem_module ) {
    verbose( VB_SHMEM, "Shared Memory Init: pid(%d), module(%s)", getpid(), shmem_module );

    /* Allocate new Shared Memory handler */
    shmem_handler_t *handler = malloc( sizeof(shmem_handler_t) );

    /* Calculate total shmem size:
     *   shsync_t will be allocated within the first SHSYNC_MAX_SIZE bytes
     *   shdata_t will use the rest
     */
    ensure( sizeof(shmem_sync_t) <= SHSYNC_MAX_SIZE,
            "Sync structure must be %d bytes maximum\n", SHSYNC_MAX_SIZE );
    handler->size = SHSYNC_MAX_SIZE + shdata_size;

    /* Get /dev/shm/ file names to create */
    char *custom_shm_key;
    parse_env_string( "LB_SHM_KEY", &custom_shm_key );

    if ( custom_shm_key != NULL ) {
        snprintf( handler->shm_filename, SHM_NAME_LENGTH, "/DLB_%s_%s", shmem_module, custom_shm_key );
    } else {
        key_t key = getuid();
        snprintf( handler->shm_filename, SHM_NAME_LENGTH, "/DLB_%s_%d", shmem_module, key );
    }

    verbose( VB_SHMEM, "Start Process Comm - creating shared mem, module(%s)", shmem_module );

    /* Obtain a file descriptor for the shmem */
    int fd = shm_open( handler->shm_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
    if ( fd == -1 ) {
        perror( "DLB PANIC: shm_open Process" );
        exit( EXIT_FAILURE );
    }

    /* Truncate the regular file to a precise size */
    if ( ftruncate( fd, handler->size ) == -1 ) {
        perror( "DLB PANIC: ftruncate Process" );
        exit( EXIT_FAILURE );
    }

    /* Map shared memory object */
    handler->addr = mmap( NULL, handler->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ( handler->addr == MAP_FAILED ) {
        perror( "DLB PANIC: mmap Process" );
        exit( EXIT_FAILURE );
    }

    /* Create Semaphore(1): Mutex */
    handler->semaphore = sem_open( handler->shm_filename, O_CREAT, S_IRUSR | S_IWUSR, 1 );
    if ( handler->semaphore == SEM_FAILED ) {
        perror( "DLB_PANIC: Process unable to create/attach to semaphore" );
        exit( EXIT_FAILURE );
    }

    verbose( VB_SHMEM, "Start Process Comm - shared mem created, module(%s)", shmem_module );

    /* Set the address for both structs */
    handler->shsync = (shmem_sync_t*) handler->addr;
    *shdata = handler->addr + SHSYNC_MAX_SIZE;

    /* Set up shsync struct: increment reference counter and initialize pthread_mutex */
    sem_wait( handler->semaphore );
    handler->shsync->nprocs++;
    if ( handler->shsync->nprocs == 1 ) {
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
        if ( pthread_mutex_init( &(handler->shsync->shmem_mutex), &attr ) ) {
            perror( "DLB ERROR: pthread_mutex_init" );
        }

        /* Destroy pthread attributes */
        if ( pthread_mutexattr_destroy( &attr ) ) {
            perror( "DLB ERROR: pthread_mutexattr_destroy" );
        }
    }
    sem_post( handler->semaphore );

    return handler;
}

void shmem_finalize( shmem_handler_t* handler ) {
#ifdef IS_BGQ_MACHINE
    // BG/Q have some problems deallocating shmem
    // It will be cleaned after the job completion anyway
    return;
#endif

    /* Decrement reference counter */
    sem_wait( handler->semaphore );
    int nprocs = --(handler->shsync->nprocs);
    sem_post( handler->semaphore );

    /* Only the last process destroys the pthread_mutex */
    if ( nprocs == 0 ) {
        if ( pthread_mutex_destroy( &(handler->shsync->shmem_mutex) ) ) {
            perror ( "DLB ERROR: Shared Memory mutex destroy" );
        }
    }

    /* All processes must close semaphores and unmap shmem */
    if ( sem_close( handler->semaphore ) ) { perror( "DLB ERROR: sem_close" ); }
    if ( munmap( handler->addr, handler->size ) ) { perror( "DLB_ERROR: munmap" ); }

    /* Only the last process unlinks semaphores and shmem */
    if ( nprocs == 0 ) {
        if ( sem_unlink( handler->shm_filename ) ) { perror( "DLB_ERROR: sem_unlink" ); }
        if ( shm_unlink( handler->shm_filename ) ) { perror( "DLB ERROR: shm_unlink" ); }
    }

    free( handler );
}

void shmem_lock( shmem_handler_t* handler ) {
    if ( handler->shsync != NULL ) {
        pthread_mutex_lock( &(handler->shsync->shmem_mutex) );
    } else {
        sem_wait( handler->semaphore );
    }
}

void shmem_unlock( shmem_handler_t* handler ) {
    if ( handler->shsync != NULL ) {
        pthread_mutex_unlock( &(handler->shsync->shmem_mutex) );
    } else {
        sem_post( handler->semaphore );
    }
}

char *get_shm_filename( shmem_handler_t* handler ) {
    return handler->shm_filename;
}
