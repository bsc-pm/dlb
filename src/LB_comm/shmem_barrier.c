/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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
#include "LB_comm/shmem.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/mask_utils.h"

#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    bool initialized;
    unsigned int count;
    unsigned int participants;
    unsigned int ntimes;
    pthread_barrier_t barrier;
} barrier_t;

typedef struct {
    barrier_t barriers[0];
} shdata_t;

enum { SHMEM_BARRIER_VERSION = 3 };


static int barrier_id = 0;
static int max_barriers;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static const char *shmem_name = "barrier";

static barrier_t* get_barrier() {
    return &shdata->barriers[barrier_id];
}

void shmem_barrier__init(const char *shmem_key) {
    if (shm_handler != NULL) return;

    if (thread_spd && thread_spd->options.barrier_id > 0) {
        barrier_id = thread_spd->options.barrier_id;
    } else {
        barrier_id = 0;
    }

    max_barriers = mu_get_system_size();

    shmem_handler_t *init_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(barrier_t)*max_barriers,
            shmem_name, shmem_key, SHMEM_BARRIER_VERSION);

    shmem_lock_maintenance(init_handler);
    {
        barrier_t *barrier = get_barrier();

        if (barrier->count != 0) {
            shmem_unlock_maintenance(init_handler);
            fatal("Barrier Shared memory inconsistency. Initializing Shared Memory "
                    "while Barrier is un use (count = %d).\n"
                    "Please, report at " PACKAGE_BUGREPORT, barrier->count);
        }

        /* If barrier was already created, destroy it */
        if (barrier->initialized) {
            pthread_barrier_destroy(&barrier->barrier);
        }

        /* Update participants */
        ++barrier->participants;

        /* Initialize barrier with the number of participants updated */
        pthread_barrierattr_t attr;
        pthread_barrierattr_init(&attr);
        pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_barrier_init(&barrier->barrier, &attr, barrier->participants);
        pthread_barrierattr_destroy(&attr);

        /* Initialize other fields */
        barrier->count = 0;
        barrier->initialized = true;
    }
    shmem_unlock_maintenance(init_handler);

    verbose(VB_BARRIER, "Barrier Module initialized. Participants: %d",
            get_barrier()->participants);

    // Global variable is only assigned after the initialization
    shm_handler = init_handler;
}

void shmem_barrier_ext__init(const char *shmem_key) {
    max_barriers = mu_get_system_size();
    shm_handler = shmem_init((void**)&shdata, shmem_barrier__size(),
            shmem_name, shmem_key, SHMEM_BARRIER_VERSION);
}

void shmem_barrier__finalize(void) {
    if (shm_handler == NULL) return;

    verbose(VB_BARRIER, "Finalizing Barrier Module");

    shmem_lock_maintenance(shm_handler);
    {
        barrier_t *barrier = get_barrier();

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

    shmem_finalize(shm_handler, SHMEM_DELETE);
    shm_handler = NULL;
}

int shmem_barrier_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Shared memory destruction
    shmem_finalize(shm_handler, SHMEM_DELETE);
    shm_handler = NULL;
    shdata = NULL;

    return DLB_SUCCESS;
}


void shmem_barrier__barrier(void) {
    if (unlikely(shm_handler == NULL)) return;

    barrier_t *barrier = get_barrier();

    if (unlikely(!barrier->initialized)) {
        warning("Trying to use a non initialized barrier");
        return;
    }

    /* Wait until the shared memory state can be set to BUSY */
    shmem_acquire_busy(shm_handler);

    unsigned int participant_number = __sync_add_and_fetch(&barrier->count, 1);
    bool last_in = participant_number == barrier->participants;

    verbose(VB_BARRIER, "Entering barrier%s", last_in ? " (last)" : "");

    if (last_in) {
        // Barrier
        pthread_barrier_wait(&barrier->barrier);

        // Increase ntimes counter
        __sync_add_and_fetch(&barrier->ntimes, 1);
    } else {
        // Only if this process is not the last one, act as a blocking call
        IntoBlockingCall(0, 0);

        // Barrier
        pthread_barrier_wait(&barrier->barrier);

        // Recover resources for those processes that simulated a blocking call
        OutOfBlockingCall(0);
    }

    unsigned int participants_left = __sync_sub_and_fetch(&barrier->count, 1);
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
                    "  | %12d | %12d | %12d | %12d |",
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
