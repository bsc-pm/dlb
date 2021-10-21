/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef SHMEM_H
#define SHMEM_H

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

// Shared Memory State. Used for state-based spinlocks.
typedef enum ShmemState {
    SHMEM_READY,
    SHMEM_BUSY,
    SHMEM_MAINTENANCE
} shmem_state_t;

// Shared Memory Sync. Must be a struct because it will be allocated inside the shmem
typedef struct {
    unsigned int        shsync_version; // Shared Memory Sync version, set by the first process
    unsigned int        shmem_version;  // Shared Memory version, set by the first process
    int                 initializing;   // Only the first process sets 0 -> 1
    int                 initialized;    // Only the first process sets 0 -> 1
    shmem_state_t       state;          // Shared memory state
    pthread_spinlock_t  shmem_lock;     // Spin-lock to grant exclusive access to the shmem
    pid_t               pidlist[0];     // Array of attached PIDs
} shmem_sync_t;

enum { SHMEM_SYNC_VERSION = 2 };

enum { SHM_NAME_LENGTH = 32 };

typedef struct {
    size_t          shm_size;
    char            shm_filename[SHM_NAME_LENGTH];
    char            *shm_addr;
    shmem_sync_t    *shsync;
} shmem_handler_t;

enum { SHMEM_VERSION_IGNORE = 0 };

shmem_handler_t* shmem_init(void **shdata, size_t shdata_size, const char *shmem_module,
        const char *shmem_key, unsigned int shmem_version, void (*cleanup_fn)(void*,int));
void shmem_finalize(shmem_handler_t *handler, bool (*is_empty_fn)(void));
void shmem_lock(shmem_handler_t *handler);
void shmem_unlock(shmem_handler_t *handler);
void shmem_lock_maintenance( shmem_handler_t* handler );
void shmem_unlock_maintenance( shmem_handler_t* handler );
void shmem_acquire_busy( shmem_handler_t* handler );
void shmem_release_busy( shmem_handler_t* handler );
char *get_shm_filename(shmem_handler_t *handler);
bool shmem_exists(const char *shmem_module, const char *shmem_key);
void shmem_destroy(const char *shmem_module, const char *shmem_key);
int shmem_shsync__version(void);
size_t shmem_shsync__size(void);

#endif /* SHMEM_H */
