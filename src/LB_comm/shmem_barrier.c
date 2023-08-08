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

#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    bool initialized;
    bool lewi;
    unsigned int participants;
    atomic_uint ntimes;
    atomic_uint count;
    pthread_barrier_t barrier;
} barrier_t;

typedef struct {
    barrier_t barriers[0];
} shdata_t;

enum { SHMEM_BARRIER_VERSION = 5 };

static bool * attached = NULL;
static int max_barriers;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static const char *shmem_name = "barrier";

static void cleanup_shmem(void *shdata_ptr, int pid) {
    shdata_t *shared_data = shdata_ptr;

    int i;
    for (i = 0; i < max_barriers; i++) {
        barrier_t *barrier = &shared_data->barriers[i];
        /* We don't have the shm_handler to lock in maintenance mode, just decrease */
        --barrier->participants;
    }
}

void shmem_barrier__init(const char *shmem_key) {
    if (shm_handler != NULL) return;

    max_barriers = mu_get_system_size();

    shm_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(barrier_t)*max_barriers,
            shmem_name, shmem_key, SHMEM_BARRIER_VERSION, cleanup_shmem);

    attached = calloc(max_barriers, sizeof(bool));

    verbose(VB_BARRIER, "Barrier Module initialized.");
}

void shmem_barrier_ext__init(const char *shmem_key) {
    max_barriers = mu_get_system_size();
    shm_handler = shmem_init((void**)&shdata, shmem_barrier__size(),
            shmem_name, shmem_key, SHMEM_BARRIER_VERSION, cleanup_shmem);
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

    free(attached);
    attached = NULL;

    shmem_finalize(shm_handler, NULL /* do not check if empty */);
    shm_handler = NULL;
}

int shmem_barrier_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    free(attached);
    attached = NULL;

    // Shared memory destruction
    shmem_finalize(shm_handler, NULL /* do not check if empty */);
    shm_handler = NULL;
    shdata = NULL;

    return DLB_SUCCESS;
}

int shmem_barrier__get_system_id(void) {
    return max_barriers-1;
}

int shmem_barrier__get_num_participants(int barrier_id) {
    /* FIXME: This function may return a wrong value
     * while the barrier is in maintenance mode. */
    return shdata->barriers[barrier_id].participants;
}

int shmem_barrier__attach(int barrier_id, bool lewi) {
    if (shm_handler == NULL) return DLB_ERR_UNKNOWN;
    if (barrier_id < 0 || barrier_id >= max_barriers) {
        return DLB_ERR_NOSHMEM;
    }
    if (attached[barrier_id]) return DLB_NOUPDT;

    int participants = 0;
    shmem_lock_maintenance(shm_handler);
    {
        barrier_t *barrier = &shdata->barriers[barrier_id];

        if (barrier->count != 0) {
            shmem_unlock_maintenance(shm_handler);
            fatal("Barrier Shared memory inconsistency. Initializing Shared Memory "
                    "while Barrier is in use (count = %d).\n"
                    "Please, report at " PACKAGE_BUGREPORT, barrier->count);
        }

        /* If barrier was already created, destroy it */
        if (barrier->initialized) {
            pthread_barrier_destroy(&barrier->barrier);
        }

        /* Update participants */
        participants = ++barrier->participants;

        /* Initialize barrier with the number of participants updated */
        pthread_barrierattr_t attr;
        pthread_barrierattr_init(&attr);
        pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
        pthread_barrierattr_destroy(&attr);

        /* Initialize other fields */
        barrier->count = 0;
        barrier->initialized = true;
        barrier->lewi = lewi;
    }
    shmem_unlock_maintenance(shm_handler);

    verbose(VB_BARRIER, "Attached to barrier %d. Participants: %d", barrier_id,
            participants);

    attached[barrier_id] = true;

    return DLB_SUCCESS;
}

int shmem_barrier__detach(int barrier_id) {
    if (shm_handler == NULL) return DLB_ERR_UNKNOWN;
    if (barrier_id < 0 || barrier_id >= max_barriers) {
        return DLB_ERR_NOSHMEM;
    }
    if (!attached[barrier_id]) return DLB_NOUPDT;

    shmem_lock_maintenance(shm_handler);
    {
        barrier_t *barrier = &shdata->barriers[barrier_id];

        if (barrier->count != 0) {
            shmem_unlock_maintenance(shm_handler);
            fatal("Barrier Shared memory inconsistency. Finalizing Shared Memory "
                    "while Barrier is un use (count = %d).\n"
                    "Please, report at " PACKAGE_BUGREPORT, barrier->count);
        }

        if (barrier->initialized) {
            /* Each process that finalizes must destroy the barrier */
            pthread_barrier_destroy(&barrier->barrier);

            /* If this is the last participant, uninitialize barrier */
            if (--barrier->participants == 0) {
                barrier->initialized = false;
            }
            /* Otherwise, create a new one with the number of participants updated */
            else {
                pthread_barrierattr_t attr;
                pthread_barrierattr_init(&attr);
                pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
                pthread_barrierattr_destroy(&attr);
            }
        }
    }
    shmem_unlock_maintenance(shm_handler);

    attached[barrier_id] = false;

    return DLB_SUCCESS;
}

void shmem_barrier__barrier(int barrier_id) {
    if (unlikely(shm_handler == NULL)) return;

    barrier_t *barrier = &shdata->barriers[barrier_id];

    if (unlikely(!barrier->initialized)) {
        warning("Trying to use a non initialized barrier");
        return;
    }

    /* Wait until the shared memory state can be set to BUSY */
    shmem_acquire_busy(shm_handler);

    unsigned int participant_number = DLB_ATOMIC_ADD_FETCH(&barrier->count, 1);
    bool last_in = participant_number == barrier->participants;

    verbose(VB_BARRIER, "Entering barrier%s", last_in ? " (last)" : "");

    if (last_in) {
        // Barrier
        pthread_barrier_wait(&barrier->barrier);

        // Increase ntimes counter
        DLB_ATOMIC_ADD_RLX(&barrier->ntimes, 1);
    } else {
        // Only if this process is not the last one, act as a blocking call
        if (barrier->lewi) {
            dlb_mpi_flags_t flags = (const dlb_mpi_flags_t) {
                .is_blocking = true,
                .is_collective = true,
                .lewi_mpi = true,
            };
            into_mpi(flags);
        }

        // Barrier
        pthread_barrier_wait(&barrier->barrier);

        // Recover resources for those processes that simulated a blocking call
        if (barrier->lewi) {
            dlb_mpi_flags_t flags = (const dlb_mpi_flags_t) {
                .is_blocking = true,
                .is_collective = true,
                .lewi_mpi = true,
            };
            out_of_mpi(flags);
        }
    }

    unsigned int participants_left = DLB_ATOMIC_SUB_FETCH(&barrier->count, 1);
    bool last_out = participants_left == 0;

    verbose(VB_BARRIER, "Leaving barrier%s", last_out ? " (last)" : "");

    /* WARNING: There may be a race condition here, if another process enters a new barrier
     *          before 'count' reaches 0, there won't be anyone with 'last_one' == true.
     *          But it is completely harmless.
     *          Actually, it may be even better since the shared memory does not need to go
     *          into READY mode.
     */
    /* WARNING: There may be two race conditions here:
     *          A) If another process enters a new barrier before 'count' reaches 0, there
     *          won't be anyone with 'last_out' == true.
     *          This is completely harmless and, actually, may be even beneficial since
     *          the shared memory does not need to go into READY mode.
     *          B) Some process is the 'last_out', then another one enters a barrier,
     *          then the first one changes the state BUSY -> READY, potentially allowing
     *          another process to acquire the shmem in MAINTENANCE mode.
     *          This race condition will be considered while acquiring the shmem in
     *          MAINTENANCE mode.
     */
    if (last_out) {
        /* The last process may unset the BUSY state */
        shmem_release_busy(shm_handler);
    }
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

    int bid;
    for (bid = 0; bid < max_barriers; ++bid) {
        barrier_t *barrier = &shdata_copy->barriers[bid];
        if (barrier->initialized) {

            /* Append line to buffer */
            snprintf(line, MAX_LINE_LEN,
                    "  | %12d | %12u | %12u | %12u |",
                    bid, barrier->participants, barrier->count, barrier->ntimes);
            printbuffer_append(&buffer, line);
        }
    }

    if (buffer.addr[0] != '\0' ) {
        info0("=== Barriers ===\n"
              "  |  Barrier ID  | Participants | Num. blocked | Times compl. |\n"
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
