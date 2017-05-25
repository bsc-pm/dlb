/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef SHMEM_H
#define SHMEM_H

#include <stdlib.h>
#include <pthread.h>

#define SHM_NAME_LENGTH 32

// Shared Memory Sync. Must be a struct because it will be allocated inside the shmem
typedef struct {
    int                 initializing;   // Only the first process sets 0 -> 1
    int                 initialized;    // Only the first process sets 0 -> 1
    pthread_spinlock_t  shmem_lock;     // Spin-lock to grant exclusive access to the shmem
    pid_t               pidlist[0];     // Array of attached PIDs
} shmem_sync_t;

typedef struct {
    size_t          shm_size;
    char            shm_filename[SHM_NAME_LENGTH];
    char            *shm_addr;
    shmem_sync_t    *shsync;
} shmem_handler_t;

typedef enum ShmemOption {
    SHMEM_NODELETE,
    SHMEM_DELETE
} shmem_option_t;

shmem_handler_t* shmem_init(void **shdata, size_t shdata_size, const char *shmem_module,
        const char *shmem_key);
void shmem_finalize(shmem_handler_t *handler, shmem_option_t shmem_delete);
void shmem_lock(shmem_handler_t *handler);
void shmem_unlock(shmem_handler_t *handler);
char *get_shm_filename(shmem_handler_t *handler);

#endif /* SHMEM_H */
