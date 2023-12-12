/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem_barrier.h"

#include "LB_core/DLB_kernel.h"
#include "LB_core/DLB_talp.h"
#include "LB_comm/shmem.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct barrier_flags {
    bool initialized:1;
    bool lewi:1;
} barrier_flags_t;

typedef struct barrier_t {
    char name[BARRIER_NAME_MAX];
    barrier_flags_t flags;
    unsigned int participants;
    atomic_uint ntimes;
    atomic_uint count;
    pthread_barrier_t barrier;
    pthread_rwlock_t rwlock;
} barrier_t;

typedef struct {
    barrier_t barriers[0];
} shdata_t;

enum { SHMEM_BARRIER_VERSION = 6 };
enum { SHMEM_TIMEOUT_SECONDS = 1 };

static int max_barriers;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static const char *shmem_name = "barrier";

static void cleanup_shmem(void *shdata_ptr, int pid) {
    shdata_t *shared_data = shdata_ptr;

    int i;
    for (i = 0; i < max_barriers; i++) {
        barrier_t *barrier = &shared_data->barriers[i];
        if (--barrier->participants == 0) {
            *barrier = (const barrier_t){};
        }
    }
}

void shmem_barrier__init(const char *shmem_key) {
    if (shm_handler != NULL) return;

    max_barriers = mu_get_system_size();

    shm_handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = shmem_barrier__size(),
                .name = shmem_name,
                .key = shmem_key,
                .version = SHMEM_BARRIER_VERSION,
                .cleanup_fn = cleanup_shmem,
            });

    verbose(VB_BARRIER, "Barrier Module initialized.");
}

void shmem_barrier_ext__init(const char *shmem_key) {
    max_barriers = mu_get_system_size();
    shm_handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = shmem_barrier__size(),
                .name = shmem_name,
                .key = shmem_key,
                .version = SHMEM_BARRIER_VERSION,
                .cleanup_fn = cleanup_shmem,
            });
}

void shmem_barrier__finalize(const char *shmem_key) {
    if (shm_handler == NULL) {
        /* barrier_finalize may be called to finalize existing process
         * even if the file descriptor is not opened. (DLB_PreInit + forc-exec case) */
        if (shmem_exists(shmem_name, shmem_key)) {
            shmem_barrier_ext__init(shmem_key);
        } else {
            return;
        }
    }

    verbose(VB_BARRIER, "Finalizing Barrier Module");

    shmem_finalize(shm_handler, NULL /* do not check if empty */);
    shm_handler = NULL;
}

int shmem_barrier_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Shared memory destruction
    shmem_finalize(shm_handler, NULL /* do not check if empty */);
    shm_handler = NULL;
    shdata = NULL;

    return DLB_SUCCESS;
}

int shmem_barrier__get_system_id(void) {
    return max_barriers-1;
}

int shmem_barrier__get_max_barriers(void) {
    return max_barriers;
}

/* Given a barrier_name, find whether the barrier is registered in the shared
 * memory. Note that this function is not thread-safe */
barrier_t* shmem_barrier__find(const char *barrier_name) {
    if (shm_handler == NULL) return NULL;
    if (barrier_name == NULL) return NULL;

    barrier_t *barrier = NULL;
    int i;
    for (i=0; i<max_barriers; ++i) {
        if (shdata->barriers[i].participants > 0
                && shdata->barriers[i].flags.initialized
                && strncmp(shdata->barriers[i].name, barrier_name,
                    BARRIER_NAME_MAX-1) == 0) {
            barrier = &shdata->barriers[i];
            break;
        }
    }

    return barrier;
}

/* Register and attach process to barrier.
 *   barrier_name: barrier name
 *   lewi: whether this barrier does lewi
 */
barrier_t* shmem_barrier__register(const char *barrier_name, bool lewi) {
    if (shm_handler == NULL) return NULL;
    if (barrier_name == NULL) return NULL;

    /* Obtain the shared memory lock to find the appropriate place for the new
     * barrier. If the barrier is not created, the first process must
     * initialize it before releasing the lock. If the barrier was already
     * created, new processes may attach after acquiring both the shmem and the
     * barrier specific lock. */
    int participants = -1;
    barrier_t *barrier = NULL;
    shmem_lock(shm_handler);
    {
        /* Find a new spot, or an already registered barrier */
        barrier_t *empty_spot = NULL;
        int i;
        for (i=0; i<max_barriers; ++i) {
            if (empty_spot == NULL
                    && shdata->barriers[i].participants == 0
                    && !shdata->barriers[i].flags.initialized) {
                empty_spot = &shdata->barriers[i];
            }
            else if (shdata->barriers[i].participants > 0
                    && shdata->barriers[i].flags.initialized
                    && strncmp(shdata->barriers[i].name, barrier_name,
                        BARRIER_NAME_MAX-1) == 0) {
                barrier = &shdata->barriers[i];
                break;
            }
        }

        /* New barrier, lock first and initialize required fields */
        if (barrier == NULL && empty_spot != NULL) {
            barrier = empty_spot;
            *barrier = (const barrier_t){};

            pthread_rwlockattr_t rwlockattr;
            pthread_rwlockattr_init(&rwlockattr);
            pthread_rwlockattr_setpshared(&rwlockattr, PTHREAD_PROCESS_SHARED);
            pthread_rwlock_init(&barrier->rwlock, &rwlockattr);
            pthread_rwlockattr_destroy(&rwlockattr);

            /* Initialize only once the lock is acquired */
            pthread_rwlock_wrlock(&barrier->rwlock);
            {
                barrier->flags = (const barrier_flags_t) {
                    .initialized = true,
                    .lewi = lewi,
                };
                barrier->participants = 1;
                participants = 1;
                snprintf(barrier->name, BARRIER_NAME_MAX, "%s", barrier_name);
                pthread_barrierattr_t barrierattr;
                pthread_barrierattr_init(&barrierattr);
                pthread_barrierattr_setpshared(&barrierattr, PTHREAD_PROCESS_SHARED);
                pthread_barrier_init(&barrier->barrier, &barrierattr, barrier->participants);
                pthread_barrierattr_destroy(&barrierattr);
            }
            pthread_rwlock_unlock(&barrier->rwlock);

        } else if (barrier != NULL){
            /* Barrier was created by another participant, attach.
             * (timedlock is used to avoid potential deadlocks) */
            struct timespec timeout;
            get_time_real(&timeout);
            timeout.tv_sec += SHMEM_TIMEOUT_SECONDS;
            int timedwrlock_error = pthread_rwlock_timedwrlock(&barrier->rwlock, &timeout);
            if (likely(timedwrlock_error == 0)) {
                /* Update participants */
                participants = ++barrier->participants;

                /* Create new barrier with the number of participants updated */
                pthread_barrier_destroy(&barrier->barrier);
                pthread_barrierattr_t attr;
                pthread_barrierattr_init(&attr);
                pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
                pthread_barrierattr_destroy(&attr);

                pthread_rwlock_unlock(&barrier->rwlock);

            } else if (timedwrlock_error == ETIMEDOUT || timedwrlock_error == EDEADLK) {
                shmem_unlock(shm_handler);
                fatal("Timed out while creating shmem_barrier.\n"
                        "Please, report at " PACKAGE_BUGREPORT);
            } else if (timedwrlock_error == EINVAL) {
                shmem_unlock(shm_handler);
                fatal("Error acquiring timedwrlock while creating shmem_barrier.\n"
                        "Please, report at " PACKAGE_BUGREPORT);
            }
        }
    }
    shmem_unlock(shm_handler);

    if (barrier != NULL) {
        /* Attach if needed and print number of participants */
        verbose(VB_BARRIER, "Attached to barrier %s. Participants: %d",
                barrier->name, participants);
    }

    return barrier;
}

/* The attach function should not have any races with the global shared memory.
 * At most, 'barrier' points to an unitialized barrier and error is returned.
 * */
int shmem_barrier__attach(barrier_t *barrier) {
    if (barrier == NULL) return DLB_ERR_UNKNOWN;
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

#ifdef DEBUG_VERSION
    int count;
#endif
    int participants;
    pthread_rwlock_wrlock(&barrier->rwlock);
    {
        if (!barrier->flags.initialized
                || barrier->participants == 0) {
            pthread_rwlock_unlock(&barrier->rwlock);
            return DLB_ERR_PERM;
        }

        /* Update participants */
        participants = ++barrier->participants;

        /* Create new barrier with the number of participants updated */
        pthread_barrier_destroy(&barrier->barrier);
        pthread_barrierattr_t attr;
        pthread_barrierattr_init(&attr);
        pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
        pthread_barrierattr_destroy(&attr);

#ifdef DEBUG_VERSION
        count = barrier->count;
#endif
    }
    pthread_rwlock_unlock(&barrier->rwlock);

    ensure(count == 0, "Barrier Shared memory inconsistency while attaching "
            "barrier (count = %d).\n" "Please, report at " PACKAGE_BUGREPORT,
            count);

    return participants;
}

/* The detach function may remove the barrier if 'participants' reaches 0 and
 * compete with a barrier creation. This function needs to acquire both locks. */
int shmem_barrier__detach(barrier_t *barrier) {
    if (barrier == NULL) return DLB_ERR_UNKNOWN;
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

#ifdef DEBUG_VERSION
    int count = -1;
#endif
    int participants = -1;

    shmem_lock(shm_handler);
    {
        struct timespec timeout;
        get_time_real(&timeout);
        timeout.tv_sec += SHMEM_TIMEOUT_SECONDS;
        int timedwrlock_error = pthread_rwlock_timedwrlock(&barrier->rwlock, &timeout);
        if (likely(timedwrlock_error == 0)) {

            if (!barrier->flags.initialized
                    || barrier->participants == 0) {
                pthread_rwlock_unlock(&barrier->rwlock);
                shmem_unlock(shm_handler);
                return DLB_ERR_PERM;
            }

#ifdef DEBUG_VERSION
            count = barrier->count;
#endif
            /* Both locks are acquired and barrier is valid, detach: */

            /* Update participants */
            participants = --barrier->participants;

            if (participants > 0) {
                /* Create new barrier with the number of participants updated */
                pthread_barrier_destroy(&barrier->barrier);
                pthread_barrierattr_t attr;
                pthread_barrierattr_init(&attr);
                pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
                pthread_barrierattr_destroy(&attr);

                pthread_rwlock_unlock(&barrier->rwlock);
            } else {
                /* If this is the last participant, uninitialize barrier */
                pthread_rwlock_unlock(&barrier->rwlock);
                pthread_barrier_destroy(&barrier->barrier);
                pthread_rwlock_destroy(&barrier->rwlock);
                *barrier = (const barrier_t){};
            }
        } else if (timedwrlock_error == ETIMEDOUT || timedwrlock_error == EDEADLK) {
            shmem_unlock(shm_handler);
            fatal("Timed out while detaching barrier.\n"
                    "Please, report at " PACKAGE_BUGREPORT);
        } else if (timedwrlock_error == EINVAL) {
            shmem_unlock(shm_handler);
            fatal("Error acquiring timedwrlock while detaching barrier.\n"
                    "Please, report at " PACKAGE_BUGREPORT);
        }
    }
    shmem_unlock(shm_handler);

    ensure(count == 0, "Barrier Shared memory inconsistency while attaching "
            "barrier (count = %d).\n" "Please, report at " PACKAGE_BUGREPORT,
            count);

    return participants;
}

void shmem_barrier__barrier(barrier_t *barrier) {
    if (unlikely(shm_handler == NULL)) return;

    pthread_rwlock_rdlock(&barrier->rwlock);
    {
        if (unlikely(!barrier->flags.initialized)) {
            warning("Trying to use a non initialized barrier");
            pthread_rwlock_unlock(&barrier->rwlock);
            return;
        }

        unsigned int participant_number = DLB_ATOMIC_ADD_FETCH(&barrier->count, 1);
        bool last_in = participant_number == barrier->participants;

        verbose(VB_BARRIER, "Entering barrier %s%s", barrier->name, last_in ? " (last)" : "");

        if (last_in) {
            // Barrier
            pthread_barrier_wait(&barrier->barrier);

            // Increase ntimes counter
            DLB_ATOMIC_ADD_RLX(&barrier->ntimes, 1);
        } else {
            // Only if this process is not the last one, act as a blocking call
            if (barrier->flags.lewi) {
                sync_call_flags_t mpi_flags = (const sync_call_flags_t) {
                    .is_dlb_barrier = true,
                    .is_blocking = true,
                    .is_collective = true,
                    .do_lewi = true,
                };
                into_sync_call(mpi_flags);
            }

            // Barrier
            pthread_barrier_wait(&barrier->barrier);

            // Recover resources for those processes that simulated a blocking call
            if (barrier->flags.lewi) {
                sync_call_flags_t mpi_flags = (const sync_call_flags_t) {
                    .is_dlb_barrier = true,
                    .is_blocking = true,
                    .is_collective = true,
                    .do_lewi = true,
                };
                out_of_sync_call(mpi_flags);
            }
        }

        unsigned int participants_left = DLB_ATOMIC_SUB_FETCH(&barrier->count, 1);
        bool last_out = participants_left == 0;

        verbose(VB_BARRIER, "Leaving barrier%s%s", barrier->name, last_out ? " (last)" : "");

        /* WARNING: There may be a race condition with the 'count' value in
         *          consecutive barriers (A -> B), if one process increases the
         *          'count' value after entering B, while another process still
         *          hasn't decreased it in B. Anyway, this is completely harmless
         *          since it only affects the verbose message.
         */
    }
    pthread_rwlock_unlock(&barrier->rwlock);
}

void shmem_barrier__print_info(const char *shmem_key) {

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_barrier_ext__init(shmem_key);
    }

    /* Make a full copy of the shared memory */
    shdata_t *shdata_copy = malloc(shmem_barrier__size());
    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, shmem_barrier__size());
    }
    shmem_unlock(shm_handler);

    /* Close shmem if needed */
    if (temporary_shmem) {
        shmem_barrier_ext__finalize();
    }

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up line buffer */
    enum { MAX_LINE_LEN = 128 };
    char line[MAX_LINE_LEN];

    int i;
    for (i = 0; i < max_barriers; ++i) {
        barrier_t *barrier = &shdata_copy->barriers[i];
        if (barrier->flags.initialized) {

            /* Append line to buffer */
            snprintf(line, MAX_LINE_LEN,
                    "  | %14s | %12u | %12u | %12u |",
                    barrier->name, barrier->participants, barrier->count, barrier->ntimes);
            printbuffer_append(&buffer, line);
        }
    }

    if (buffer.addr[0] != '\0' ) {
        info0("=== Barriers ===\n"
              "  |  Barrier Name  | Participants | Num. blocked | Times compl. |\n"
              "%s", buffer.addr);
    }
    printbuffer_destroy(&buffer);
    free(shdata_copy);
}

bool shmem_barrier__exists(void) {
    return shm_handler != NULL;
}

int shmem_barrier__version(void) {
    return SHMEM_BARRIER_VERSION;
}

size_t shmem_barrier__size(void) {
    return sizeof(shdata_t) + sizeof(barrier_t)*mu_get_system_size();
}
