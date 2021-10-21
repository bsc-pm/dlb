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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#error This system does not support process shared mutexes
#endif

#include "LB_comm/shmem.h"
#include "support/debug.h"
#include "support/options.h"
#include "support/mytime.h"
#include "support/mask_utils.h"

#define SHMEM_TIMEOUT_SECONDS 1

static bool shmem_consistency_check_pids(pid_t *pidlist, pid_t pid,
        void (*cleanup_fn)(void*,int), void *shdata) {
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
                /* Process pidlist[i] is registered and does not exist */
                if (cleanup_fn) {
                    verbose(VB_SHMEM,
                            "Process %d is registered in DLB but does not exist, probably"
                            " due to a bad termination of such process.\n"
                            "DLB is cleaning up the shared memory. If it fails,"
                            " please run 'dlb_shm --delete' and try again.", pidlist[i]);
                    cleanup_fn(shdata, pidlist[i]);
                    pidlist[i] = 0;
                } else {
                    verbose(VB_SHMEM, "Process %d attached to shmem not found, "
                            "you may want to run \"dlb_shm -d\"", pidlist[i]);
                }
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

void shmem_consistency_check_version(unsigned int creator_version, unsigned int process_version) {
    if (creator_version != SHMEM_VERSION_IGNORE) {
        fatal_cond(creator_version != process_version,
                "The existing DLB shared memory version differs from the expected one.\n"
                "This may have been caused by a DLB version upgrade in between runs.\n"
                "Please, run 'dlb_shm --delete' and try again.\n"
                "Contact us at " PACKAGE_BUGREPORT " if the issue persists.");
    }
}


shmem_handler_t* shmem_init(void **shdata, size_t shdata_size, const char *shmem_module,
        const char *shmem_key, unsigned int shmem_version, void (*cleanup_fn)(void*,int)) {
    pid_t pid = getpid();
    verbose(VB_SHMEM, "Shared Memory Init: pid(%d), module(%s)", pid, shmem_module);

    /* Allocate new Shared Memory handler */
    shmem_handler_t *handler = malloc(sizeof(shmem_handler_t));

    /* Calculate total shmem size:
     *   shmem = shsync + shdata
     *   shsync and shdata are both variable in size
     */
    size_t shsync_size = shmem_shsync__size();
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
        /* Shared Memory creator */
        verbose(VB_SHMEM, "Initializing Shared Memory (%s)", shmem_module);

        /* Init pthread mutex */
        pthread_mutexattr_t attr;
        if (pthread_mutexattr_init(&attr) != 0) {
            fatal("pthread_mutexattr_init error: %s", strerror(errno));
        }
        if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
            fatal("pthread_mutexattr_setpshared error: %s", strerror(errno));
        }
        if (pthread_mutex_init(&handler->shsync->shmem_mutex, &attr) != 0) {
            fatal("pthread_mutex_init error: %s", strerror(errno));
        }
        if (pthread_mutexattr_destroy(&attr) != 0) {
            fatal("pthread_mutexattr_destroy error: %s", strerror(errno));
        }

        /* Set Shared Memory version */
        handler->shsync->shmem_version = shmem_version;
        handler->shsync->shsync_version = SHMEM_SYNC_VERSION;

        handler->shsync->initialized = 1;
    } else {
        /* Shared Memory already created */
        while(!handler->shsync->initialized) __sync_synchronize();
        verbose(VB_SHMEM, "Attached to Shared Memory (%s)", shmem_module);
    }

    /* Check consistency */
    verbose(VB_SHMEM, "Checking shared memory consistency (%s)", shmem_module);
    struct timespec timeout;
    get_time_real(&timeout);
    timeout.tv_sec += SHMEM_TIMEOUT_SECONDS;
    int error = pthread_mutex_timedlock(&handler->shsync->shmem_mutex, &timeout);
    if (error == ETIMEDOUT) {
        fatal("DLB cannot obtain the lock for the shared memory.\n"
                "This may have been caused by a previous process crashing"
                " while acquiring the DLB shared memory lock.\n"
                "Please, run 'dlb_shm --delete' and try again.\n"
                "Contact us at " PACKAGE_BUGREPORT " if the issue persists.");
    } else if (error != 0) {
        fatal("pthread_mutex_timedlock error: %s", strerror(error));
    }
    shmem_consistency_check_version(handler->shsync->shsync_version, SHMEM_SYNC_VERSION);
    shmem_consistency_check_version(handler->shsync->shmem_version, shmem_version);
    shmem_consistency_check_pids(handler->shsync->pidlist, pid, cleanup_fn, *shdata);
    pthread_mutex_unlock(&handler->shsync->shmem_mutex);

    return handler;
}

void shmem_finalize(shmem_handler_t* handler, bool (*is_empty_fn)(void)) {
#ifdef IS_BGQ_MACHINE
    // BG/Q have some problems deallocating shmem
    // It will be cleaned after the job completion anyway
    return;
#endif

    shmem_lock(handler);
    bool is_empty = is_empty_fn ? is_empty_fn() : true;
    bool is_last_one = shmem_consistency_remove_pid(handler->shsync->pidlist, getpid());
    bool delete_shmem = is_empty && is_last_one;
    shmem_unlock(handler);

    /* Only the last process destroys the pthread_mutex */
    if (delete_shmem) {
        int error = pthread_mutex_destroy(&handler->shsync->shmem_mutex);
        fatal_cond(error, "pthread_mutex_destroy error: %s", strerror(error));
    }

    /* All processes must unmap shmem */
    if (munmap(handler->shm_addr, handler->shm_size) != 0) {
        fatal("munmap error: %s", strerror(errno));
    }

    /* Only the last process unlinks shmem */
    if (delete_shmem) {
        verbose(VB_SHMEM, "Removing shared memory %s", handler->shm_filename);
        if (shm_unlink(handler->shm_filename) != 0 && errno != ENOENT) {
            fatal("shm_unlink error: %s", strerror(errno));
        }
    }

    free(handler);
}

void shmem_lock( shmem_handler_t* handler ) {
    pthread_mutex_lock(&handler->shsync->shmem_mutex);
}

void shmem_unlock( shmem_handler_t* handler ) {
    pthread_mutex_unlock(&handler->shsync->shmem_mutex);
}

/* Shared memory states    (BUSY(0-n)  <-  READY(0-n)  ->  MAINTENANCE(1)):
 *  - READY: the shared memory can be locked or moved to another state.
 *  - BUSY: the shared memory is being used, each process still needs to
 *          use atomic operations or locks to access the data.
 *          This state prevents going to maintenance mode.
 *  - MAINTENANCE: only one process can set the state to maintenance,
 *                 processes trying to set BUSY state will have to wait
 *
 *  This system is useful if all processes want to begin a group operation
 *  (like a barrier) entering the BUSY state, and we want to prevent a new
 *  process to join the group until the shared memory goes back to READY.
 */

enum { SHMEM_TRYAQUIRE_USECS = 100 };

/* Busy wait until the shmem can be locked with the state MAINTENANCE */
void shmem_lock_maintenance( shmem_handler_t* handler ) {
    volatile shmem_state_t *state = &handler->shsync->state;
    while(1) {
        pthread_mutex_lock(&handler->shsync->shmem_mutex);
        switch(*state) {
            case SHMEM_READY:
                /* Lock successfully acquired: READY -> MAINTENANCE */
                *state = SHMEM_MAINTENANCE;
                return;
            case SHMEM_BUSY:
                /* Shmem cannot be put in maintenance while BUSY */
                pthread_mutex_unlock(&handler->shsync->shmem_mutex);
                usleep(SHMEM_TRYAQUIRE_USECS);
                break;
            case SHMEM_MAINTENANCE:
                /* This should not happen */
                pthread_mutex_unlock(&handler->shsync->shmem_mutex);
                fatal("Shared memory lock inconsistency. Please report to " PACKAGE_BUGREPORT);
                break;
        }
    }
}

/* Unlock a previoulsy shmem in the MAINTENANCE state */
void shmem_unlock_maintenance( shmem_handler_t* handler ) {
    /* Unlock MAINTENANCE -> READY */
    int error = handler->shsync->state != SHMEM_MAINTENANCE;
    handler->shsync->state = SHMEM_READY;
    pthread_mutex_unlock(&handler->shsync->shmem_mutex);

    /* This should not happen */
    fatal_cond(error, "Shared memory lock inconsistency. Please report to " PACKAGE_BUGREPORT);
}

/* Busy wait until the shmem can be set READY -> BUSY */
void shmem_acquire_busy( shmem_handler_t* handler ) {
    volatile shmem_state_t *state = &handler->shsync->state;
    while ( unlikely(
                *state != SHMEM_BUSY
                && !__sync_bool_compare_and_swap(state, SHMEM_READY, SHMEM_BUSY)
                )) {
        usleep(SHMEM_TRYAQUIRE_USECS);
    }
}

/* Set shmm state BUSY -> READY */
void shmem_release_busy( shmem_handler_t* handler ) {
    fatal_cond(
            !__sync_bool_compare_and_swap(&handler->shsync->state, SHMEM_BUSY, SHMEM_READY),
            "Shared memory lock inconsistency. Please report to " PACKAGE_BUGREPORT);
}

char *get_shm_filename( shmem_handler_t* handler ) {
    return handler->shm_filename;
}

bool shmem_exists(const char *shmem_module, const char *shmem_key) {
    char shm_filename[SHM_NAME_LENGTH*2];
    if (shmem_key && shmem_key[0] != '\0') {
        snprintf(shm_filename, SHM_NAME_LENGTH*2, "/dev/shm/DLB_%s_%s", shmem_module, shmem_key);
    } else {
        snprintf(shm_filename, SHM_NAME_LENGTH*2, "/dev/shm/DLB_%s_%d", shmem_module, getuid());
    }
    return access(shm_filename, F_OK) != -1;
}

void shmem_destroy(const char *shmem_module, const char *shmem_key) {
    char shm_filename[SHM_NAME_LENGTH*2];
    if (shmem_key && shmem_key[0] != '\0') {
        snprintf(shm_filename, SHM_NAME_LENGTH*2, "/dev/shm/DLB_%s_%s", shmem_module, shmem_key);
    } else {
        snprintf(shm_filename, SHM_NAME_LENGTH*2, "/dev/shm/DLB_%s_%d", shmem_module, getuid());
    }
    shm_unlink(shm_filename);
}

int shmem_shsync__version(void) {
    return SHMEM_SYNC_VERSION;
}

size_t shmem_shsync__size(void) {
    size_t shsync_size = sizeof(shmem_sync_t) + sizeof(pid_t) * mu_get_system_size();
    shsync_size = (shsync_size + 7) & ~7; // round up to 8 bytes
    return shsync_size;
}
