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

#include "LB_comm/shmem.h"

#include "support/debug.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>      /* For mode constants */
#include <fcntl.h>         /* For O_* constants */
#include <signal.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#ifndef _POSIX_THREAD_PROCESS_SHARED
#error This system does not support process shared spinlocks
#endif

#include "LB_comm/shmem.h"
#include "support/debug.h"
#include "support/options.h"
#include "support/mytime.h"
#include "support/mask_utils.h"

#define MAX_PATHNAME 64
#define SHMEM_TIMEOUT_SECONDS 1

static bool shmem_consistency_add_pid(pid_t *pidlist, pid_t pid) {
    bool registered = false;
    int i;
    for(i=0; i<mu_get_system_size(); ++i) {
        if (pidlist[i] == 0) {
            if (!registered) {
                pidlist[i] = pid;
                registered = true;
            }
        } else {
            if (kill(pidlist[i], 0) == -1) {
                warning("Process %d attached to shmem not found, "
                        "you may want to run \"dlb_shm -d\"", pidlist[i]);
            }
        }
    }
    return registered;
}

static bool shmem_consistency_remove_pid(pid_t *pidlist, pid_t pid) {
    bool last_one = true;
    int i;
    for(i=0; i<mu_get_system_size(); ++i) {
        if (pidlist[i] == pid) {
            pidlist[i] = 0;
        } else if (pidlist[i] != 0) {
            last_one = false;
        }
    }
    return last_one;
}


shmem_handler_t* shmem_init(void **shdata, size_t shdata_size, const char *shmem_module,
        const char *shmem_key) {
    int error;
    pid_t pid = getpid();
    verbose(VB_SHMEM, "Shared Memory Init: pid(%d), module(%s)", pid, shmem_module);

    /* Allocate new Shared Memory handler */
    shmem_handler_t *handler = malloc(sizeof(shmem_handler_t));

    /* Calculate total shmem size:
     *   shmem = shsync + shdata
     *   shsync and shdata are both variable in size
     */
    size_t shsync_size = sizeof(shmem_sync_t) + sizeof(pid_t) * mu_get_system_size();
    shsync_size = (shsync_size + 7) & ~7; // round up to 8 bytes
    handler->shm_size = shsync_size + shdata_size;

    /* Get /dev/shm/ file names to create */
    if (shmem_key && shmem_key[0] != '\0') {
        snprintf(handler->shm_filename, SHM_NAME_LENGTH, "/DLB_%s_%s", shmem_module, shmem_key);
    } else {
        snprintf(handler->shm_filename, SHM_NAME_LENGTH, "/DLB_%s_%d", shmem_module, getuid());
    }

    /* Obtain a file descriptor for the shmem */
    int fd = shm_open(handler->shm_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        fatal("shm_open error: %s", strerror(errno));
    }

    /* Truncate the regular file to a precise size */
    if (ftruncate(fd, handler->shm_size) == -1) {
        fatal("ftruncate error: %s", strerror(errno));
    }

    /* Map shared memory object */
    handler->shm_addr = mmap(NULL, handler->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (handler->shm_addr == MAP_FAILED) {
        fatal("mmap error: %s",  strerror(errno));
    }

    /* Set the address for both structs */
    handler->shsync = (shmem_sync_t*) handler->shm_addr;
    *shdata = handler->shm_addr + shsync_size;

    if (__sync_bool_compare_and_swap(&handler->shsync->initializing, 0, 1)) {
        verbose(VB_SHMEM, "Initializing Shared Memory (%s)", shmem_module);

        /* Init pthread spinlock */
        error = pthread_spin_init(&handler->shsync->shmem_lock, PTHREAD_PROCESS_SHARED);
        fatal_cond(error, "pthread_spin_init error: %s", strerror(error));

        handler->shsync->initialized = 1;
    } else {
        while(!handler->shsync->initialized) __sync_synchronize();
        verbose(VB_SHMEM, "Attached to Shared Memory (%s)", shmem_module);
    }

    /* Check consistency */
    verbose(VB_SHMEM, "Checking shared memory consistency (%s)", shmem_module);
    struct timespec start;
    get_time_coarse(&start);
    // sort of spinlock_timedlock
    while (pthread_spin_trylock(&handler->shsync->shmem_lock) != 0) {
        struct timespec now;
        get_time_coarse(&now);
        if (timespec_diff(&start, &now) > SHMEM_TIMEOUT_SECONDS * 1e9) {
            // timeout
            fatal("Could not acquire lock for the new attached shared memory. "
                    "You may want to clean it with \"dlb_shm -d\"");
        }
    }
    shmem_consistency_add_pid(handler->shsync->pidlist, pid);
    pthread_spin_unlock(&handler->shsync->shmem_lock);

    return handler;
}

void shmem_finalize(shmem_handler_t* handler, shmem_option_t shmem_delete) {
#ifdef IS_BGQ_MACHINE
    // BG/Q have some problems deallocating shmem
    // It will be cleaned after the job completion anyway
    return;
#endif

    shmem_lock(handler);
    bool last_one = shmem_consistency_remove_pid(handler->shsync->pidlist, getpid());
    shmem_unlock(handler);

    /* Only the last process destroys the pthread_mutex */
    if (last_one && shmem_delete == SHMEM_DELETE) {
        int error = pthread_spin_destroy(&handler->shsync->shmem_lock);
        fatal_cond(error, "pthread_mutex_destroy error: %s", strerror(error));
    }

    /* All processes must unmap shmem */
    if (munmap(handler->shm_addr, handler->shm_size) != 0) {
        fatal("munmap error: %s", strerror(errno));
    }

    /* Only the last process unlinks shmem */
    if (last_one && shmem_delete == SHMEM_DELETE) {
        verbose(VB_SHMEM, "Removing shared memory %s", handler->shm_filename);
        if (shm_unlink(handler->shm_filename) != 0) {
            fatal("shm_unlink error: %s", strerror(errno));
        }
    }

    free(handler);
}

void shmem_lock( shmem_handler_t* handler ) {
    pthread_spin_lock(&handler->shsync->shmem_lock);
}

void shmem_unlock( shmem_handler_t* handler ) {
    pthread_spin_unlock(&handler->shsync->shmem_lock);
}

char *get_shm_filename( shmem_handler_t* handler ) {
    return handler->shm_filename;
}
