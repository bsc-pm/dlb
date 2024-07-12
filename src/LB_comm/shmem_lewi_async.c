/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem_lewi_async.h"

#include "LB_comm/shmem.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/queues.h"
#include "support/tracing.h"
#include "support/types.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct DLB_ALIGN_CACHE lewi_process_t {
    pid_t           pid;
    unsigned int    initial_ncpus;
    unsigned int    current_ncpus;
} lewi_process_t;

typedef struct lewi_async_shdata {
    unsigned int        idle_cpus;
    unsigned int        attached_nprocs;
    queue_lewi_reqs_t   requests;       /* queue of requests */
    unsigned int        proc_list_head;
    lewi_process_t      processes[];    /* per-process lewi data */
} lewi_async_shdata_t;

enum { NOBODY = 0 };
enum { SHMEM_LEWI_ASYNC_VERSION = 2 };

static lewi_async_shdata_t *shdata = NULL;
static shmem_handler_t *shm_handler = NULL;
static const char *shmem_name = "lewi_async";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;
static int max_processes;
static lewi_process_t *my_process = NULL;


static void lend_ncpus_to_shmem(unsigned int ncpus, lewi_request_t *requests,
        unsigned int *nreqs, unsigned int maxreqs);
static int reclaim_from_shmem(lewi_process_t *process, unsigned int ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs);

/*********************************************************************************/
/*  Shared memory management                                                     */
/*********************************************************************************/

static void cleanup_shmem(void *shdata_ptr, int pid) {
    lewi_async_shdata_t *shared_data = shdata_ptr;
    if (shared_data->attached_nprocs > 0) {
        --shared_data->attached_nprocs;
    }
}

static bool is_shmem_empty(void) {
    return shdata && shdata->attached_nprocs == 0;
}

static lewi_process_t* get_process(pid_t pid) {
    if (shdata != NULL) {
        /* Check first if pid is this process */
        if (my_process != NULL
                && my_process->pid == pid) {
            return my_process;
        }

        /* Iterate otherwise */
        for (unsigned int p = 0; p < shdata->proc_list_head; ++p) {
            if (shdata->processes[p].pid == pid) {
                return &shdata->processes[p];
            }
        }
    }
    return NULL;
}

bool shmem_lewi_async__exists(void) {
    return shm_handler != NULL;
}

int shmem_lewi_async__version(void) {
    return SHMEM_LEWI_ASYNC_VERSION;
}

size_t shmem_lewi_async__size(void) {
    return sizeof(lewi_async_shdata_t) + sizeof(lewi_process_t)*mu_get_system_size();
}


/*********************************************************************************/
/*  Queue management                                                             */
/*********************************************************************************/

void shmem_lewi_async__remove_requests(pid_t pid) {
    if (unlikely(shm_handler == NULL)) return;
    shmem_lock(shm_handler);
    {
        queue_lewi_reqs_remove(&shdata->requests, pid);
    }
    shmem_unlock(shm_handler);
}

/* Only for testing purposes */
unsigned int shmem_lewi_async__get_num_requests(pid_t pid) {
    if (unlikely(shm_handler == NULL)) return 0;
    unsigned int num_requests;

    shmem_lock(shm_handler);
    {
        num_requests = queue_lewi_reqs_get(&shdata->requests, pid);
    }
    shmem_unlock(shm_handler);

    return num_requests;
}


/*********************************************************************************/
/*  Init                                                                         */
/*********************************************************************************/

static void open_shmem(const char *shmem_key) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            shm_handler = shmem_init((void**)&shdata,
                    &(const shmem_props_t) {
                        .size = shmem_lewi_async__size(),
                        .name = shmem_name,
                        .key = shmem_key,
                        .version = SHMEM_LEWI_ASYNC_VERSION,
                        .cleanup_fn = cleanup_shmem,
                    });
            subprocesses_attached = 1;
            max_processes = mu_get_system_size();
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void shmem_lewi_async__init(pid_t pid, unsigned int ncpus, const char *shmem_key) {
    verbose(VB_SHMEM, "Initializing LeWI_async shared memory");

    // Shared memory creation
    open_shmem(shmem_key);

    shmem_lock(shm_handler);
    {
        if (++shdata->attached_nprocs == 1) {
            // first attached process, initialize common structures
            shdata->idle_cpus = 0;
            queue_lewi_reqs_init(&shdata->requests);
        }

        // Iterate the processes array to find a free spot
        lewi_process_t *process = NULL;
        for (int p = 0; p < max_processes; ++p) {
            if (shdata->processes[p].pid == NOBODY) {
                process = &shdata->processes[p];
                /* save the highest upper bound to iterate faster */
                shdata->proc_list_head = max_int(shdata->proc_list_head, p+1);
                break;
            }
        }

        // Assign this process initial data
        if (process != NULL) {
            *process = (const lewi_process_t) {
                .pid = pid,
                .initial_ncpus = ncpus,
                .current_ncpus = ncpus,
            };
            my_process = process;
        }
    }
    shmem_unlock(shm_handler);
}


/*********************************************************************************/
/*  Finalize                                                                     */
/*********************************************************************************/

static int reset_process(lewi_process_t *process, lewi_request_t *requests,
        unsigned int *nreqs, unsigned int maxreqs, unsigned int *prev_requested) {

    *nreqs = 0;
    int error = DLB_NOUPDT;

    // Clear requests
    *prev_requested = queue_lewi_reqs_remove(&shdata->requests, process->pid);

    // Lend excess CPUs
    if (process->current_ncpus > process->initial_ncpus) {

        /* Compute CPUs to lend and update process info */
        unsigned int ncpus_to_lend =
            process->current_ncpus - process->initial_ncpus;
        process->current_ncpus = process->initial_ncpus;

        /* Update output variable. Excess CPUs count as previously requested */
        *prev_requested += ncpus_to_lend;

        /* Update shmem, resolve requests if possible */
        lend_ncpus_to_shmem(ncpus_to_lend, requests, nreqs, maxreqs);

        error = DLB_SUCCESS;
    }

    // Borrow or Reclaim
    else if (process->current_ncpus < process->initial_ncpus) {

        unsigned int ncpus_to_reclaim =
            process->initial_ncpus - process->current_ncpus;

        // Borrow first
        unsigned int ncpus_to_borrow = min_uint(shdata->idle_cpus,
                ncpus_to_reclaim);
        if (ncpus_to_borrow > 0) {
            shdata->idle_cpus -= ncpus_to_borrow;
            process->current_ncpus += ncpus_to_borrow;
            ncpus_to_reclaim -= ncpus_to_borrow;
            error = DLB_SUCCESS;
        }

        // Reclaim later
        if (ncpus_to_reclaim > 0) {
            error = reclaim_from_shmem(process, ncpus_to_reclaim, requests,
                    nreqs, maxreqs);
        }
    }

    return error;
}

static void close_shmem(void) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, is_shmem_empty);
            shm_handler = NULL;
            shdata = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void shmem_lewi_async__finalize(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs) {

    *nreqs = 0;

    if (shm_handler == NULL) return;

    lewi_process_t *process = get_process(pid);

    shmem_lock(shm_handler);
    {
        if (process != NULL) {
            *new_ncpus = process->initial_ncpus;

            // Resolve requests and CPUs out of place, ignore previously requested
            unsigned int prev_requested;
            reset_process(process, requests, nreqs, maxreqs, &prev_requested);

            // Remove process data
            *process = (const lewi_process_t) {};

            // Clear local pointer
            my_process = NULL;

            // Decrement process counter
            --shdata->attached_nprocs;
        }
    }
    shmem_unlock(shm_handler);

    close_shmem();
}


/*********************************************************************************/
/*  Lend                                                                         */
/*********************************************************************************/

/* Lend ncpus. Check whether they can resolve some request, add to shmem otherwise */
static void lend_ncpus_to_shmem(unsigned int ncpus, lewi_request_t *requests,
        unsigned int *nreqs, unsigned int maxreqs) {

    if (queue_lewi_reqs_size(&shdata->requests) == 0) {
        /* queue is empty */
        shdata->idle_cpus += ncpus;
        *nreqs = 0;
    } else {

        /* Resolve as many requests as possible, the remainder goes to the shmem */
        unsigned int not_needed_cpus = queue_lewi_reqs_pop_ncpus(&shdata->requests,
                ncpus, requests, nreqs, maxreqs);
        shdata->idle_cpus += not_needed_cpus;

        /* Update shmem with the resolved requests, and add current values */
        for (unsigned int i = 0; i < *nreqs; ++i) {
            lewi_process_t *target = get_process(requests[i].pid);
            target->current_ncpus += requests[i].howmany;
            /* the request is updated to call the appropriate set_num_threads */
            requests[i].howmany = target->current_ncpus;
        }
    }
}

/* Lend ncpus. Return new_ncpus and pending CPU requests. */
int shmem_lewi_async__lend_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus, lewi_request_t *requests, unsigned int *nreqs,
        unsigned int maxreqs, unsigned int *prev_requested) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    *new_ncpus = 0;
    *nreqs = 0;
    *prev_requested = 0;

    if (unlikely(ncpus == 0)) return DLB_NOUPDT;

    int error;
    unsigned int idle_cpus;
    shmem_lock(shm_handler);
    {
        /* Not enough CPUs to lend */
        if (ncpus > process->current_ncpus) {
            error = DLB_ERR_PERM;
        }
        /* Lend */
        else {
            /* CPUs previously requested is the sum of the previous petitions
             * in the queue (which are removed at this point) and the excess of
             * CPUs that the process lends but does not own */
            unsigned int ncpus_lent_not_owned =
                process->current_ncpus > process->initial_ncpus
                ? min_uint(process->current_ncpus - process->initial_ncpus, ncpus)
                : 0;
            unsigned int ncpus_in_queue = queue_lewi_reqs_remove(&shdata->requests, pid);
            *prev_requested = ncpus_lent_not_owned + ncpus_in_queue;

            /* Update process info and output variable */
            process->current_ncpus -= ncpus;
            *new_ncpus = process->current_ncpus;

            /* Update shmem, resolve requests if possible */
            lend_ncpus_to_shmem(ncpus, requests, nreqs, maxreqs);

            error = DLB_SUCCESS;
        }

        idle_cpus = shdata->idle_cpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Lending %u CPUs (idle: %u, triggered requests: %u)",
                ncpus, idle_cpus, *nreqs);
    } else {
        verbose(VB_SHMEM, "Lend failed");
    }

    return error;
}

/* Lend all possible CPUs, keep new_ncpus. Return pending CPU requests. */
int shmem_lewi_async__lend_keep_cpus(pid_t pid, unsigned int new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int *prev_requested) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    *nreqs = 0;
    *prev_requested = 0;

    int error;
    unsigned int idle_cpus;
    unsigned int lent_cpus;
    shmem_lock(shm_handler);
    {
        /* Not enough CPUs to lend */
        if (new_ncpus > process->current_ncpus) {
            error = DLB_ERR_PERM;
        }

        /* No-op */
        else if (new_ncpus == process->current_ncpus) {
            error = DLB_NOUPDT;
        }

        /* Lend */
        else {
            /* Compute CPUs to lend */
            lent_cpus = process->current_ncpus - new_ncpus;

            /* CPUs previously requested is the sum of the previous petitions
             * in the queue (which are removed at this point) and the excess of
             * CPUs that the process lends but does not own */
            unsigned int ncpus_lent_not_owned =
                process->current_ncpus > process->initial_ncpus
                ? min_uint(process->current_ncpus - process->initial_ncpus, lent_cpus)
                : 0;
            unsigned int ncpus_in_queue = queue_lewi_reqs_remove(&shdata->requests, pid);
            *prev_requested = ncpus_lent_not_owned + ncpus_in_queue;

            /* Compute CPUs to lend and update process info */
            lent_cpus = process->current_ncpus - new_ncpus;
            process->current_ncpus = new_ncpus;

            /* Update shmem, resolve requests if possible */
            lend_ncpus_to_shmem(lent_cpus, requests, nreqs, maxreqs);

            error = DLB_SUCCESS;
        }

        idle_cpus = shdata->idle_cpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Lending %u CPUs (idle: %u, triggered requests: %u)",
                lent_cpus, idle_cpus, *nreqs);
    } else {
        verbose(VB_SHMEM, "Lend failed");
    }

    return error;
}


/*********************************************************************************/
/*  Reclaim                                                                      */
/*********************************************************************************/

/* Helper function to reclaim all CPUs from a process */
static int reclaim_from_shmem(lewi_process_t *process, unsigned int ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs) {

    /* These conditions are actually checked before invoking the function */
    ensure(process != NULL, "illegal process in %s", __func__);
    ensure(process->initial_ncpus >= process->current_ncpus + ncpus,
            "cannot reclaim in %s", __func__);
    ensure(shdata->idle_cpus == 0, "reclaiming while idle CPUs > 0");
    ensure(ncpus > 0, "Reclaiming 0 CPUs");

    int error = DLB_SUCCESS;

    // find victims to steal CPUs from

    /* Construct a queue with the CPU surplus of each target process */
    queue_lewi_reqs_t surplus = {};
    for (unsigned int p = 0; p < shdata->proc_list_head; ++p) {
        lewi_process_t *target = &shdata->processes[p];
        if (target->pid != NOBODY
                && target->current_ncpus > target->initial_ncpus) {
            queue_lewi_reqs_push(&surplus, target->pid,
                    target->current_ncpus - target->initial_ncpus);
        }
    }

    /* Pop CPUs evenly */
    unsigned int remaining_ncpus = queue_lewi_reqs_pop_ncpus(&surplus, ncpus,
            requests, nreqs, maxreqs);
    if (remaining_ncpus == 0) {
        /* Update shmem with the victims, subtract current values */
        for (unsigned int i = 0; i < *nreqs; ++i) {
            lewi_process_t *target = get_process(requests[i].pid);
            target->current_ncpus -= requests[i].howmany;

            /* Add requests for reclaimed CPUs */
            queue_lewi_reqs_push(&shdata->requests, requests[i].pid,
                    requests[i].howmany);

            /* the request is updated to call the appropriate set_num_threads */
            requests[i].howmany = target->current_ncpus;
        }

        process->current_ncpus += ncpus;
    } else {
        /* This should be either an error or an impossibility to fill
            * the requests array, due to a low capacity. */
        error = DLB_ERR_REQST;
    }

    return error;
}

/* Reclaim initial number of CPUs */
int shmem_lewi_async__reclaim(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int prev_requested) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    verbose(VB_SHMEM, "Reclaiming initial CPUs...");

    *nreqs = 0;

    int error = DLB_NOUPDT;
    unsigned int idle_cpus;
    shmem_lock(shm_handler);
    {
        if (process->current_ncpus < process->initial_ncpus) {

            unsigned int ncpus_to_reclaim =
                process->initial_ncpus - process->current_ncpus;

            // Borrow first
            unsigned int ncpus_to_borrow = min_uint(shdata->idle_cpus,
                    ncpus_to_reclaim);
            if (ncpus_to_borrow > 0) {
                shdata->idle_cpus -= ncpus_to_borrow;
                process->current_ncpus += ncpus_to_borrow;
                ncpus_to_reclaim -= ncpus_to_borrow;
                error = DLB_SUCCESS;
            }

            // Reclaim later
            if (ncpus_to_reclaim > 0) {
                error = reclaim_from_shmem(process, ncpus_to_reclaim, requests,
                        nreqs, maxreqs);
            }

            // Attend previous requests
            if (prev_requested > 0) {
                // Try first to resolve them with the available CPUs
                unsigned int ncpus_from_prev_requests =
                    min_uint(shdata->idle_cpus, prev_requested);
                if (ncpus_from_prev_requests > 0) {
                    shdata->idle_cpus -= ncpus_from_prev_requests;
                    process->current_ncpus += ncpus_from_prev_requests;
                    prev_requested -= ncpus_from_prev_requests;
                }

                // If we still have previous requests, add them to the queue
                if (prev_requested > 0) {
                    queue_lewi_reqs_push(&shdata->requests, pid, prev_requested);
                }
            }
        }

        // Update output variable
        idle_cpus = shdata->idle_cpus;
        *new_ncpus = process->current_ncpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Using %u CPUs... Idle: %u", *new_ncpus, idle_cpus);
    }

    return error;
}


/*********************************************************************************/
/*  Acquire                                                                      */
/*********************************************************************************/

int shmem_lewi_async__acquire_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus, lewi_request_t *requests, unsigned int *nreqs,
        unsigned int maxreqs) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    if (ncpus == 0) return DLB_NOUPDT;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    *nreqs = 0;

    int error = DLB_NOUPDT;
    int idle_cpus;
    shmem_lock(shm_handler);
    {
        // Borrow first
        unsigned int ncpus_to_borrow = min_uint(shdata->idle_cpus, ncpus);
        if (ncpus_to_borrow > 0) {
            shdata->idle_cpus -= ncpus_to_borrow;
            process->current_ncpus += ncpus_to_borrow;
            ncpus -= ncpus_to_borrow;
            error = DLB_SUCCESS;
        }

        // Reclaim later
        unsigned int ncpus_to_reclaim = min_uint(ncpus,
                process->initial_ncpus > process->current_ncpus ?
                process->initial_ncpus - process->current_ncpus : 0);
        if (ncpus_to_reclaim > 0) {
            error = reclaim_from_shmem(process, ncpus_to_reclaim, requests,
                    nreqs, maxreqs);
            if (error == DLB_SUCCESS) {
                ncpus -= ncpus_to_reclaim;
            }
        }

        // Add request for the rest
        if (ncpus > 0) {
            queue_lewi_reqs_push(&shdata->requests, pid, ncpus);
            error = DLB_NOTED;
        }

        *new_ncpus = process->current_ncpus;
        idle_cpus = shdata->idle_cpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Borrowing CPUs... New: %d, Idle: %d", *new_ncpus, idle_cpus);
    }

    return error;
}


/*********************************************************************************/
/*  Borrow                                                                       */
/*********************************************************************************/

int shmem_lewi_async__borrow_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    int error;
    int idle_cpus;

    shmem_lock(shm_handler);
    {
        // Borrow as many as possible
        unsigned int ncpus_to_borrow = min_uint(shdata->idle_cpus, ncpus);
        if (ncpus_to_borrow > 0) {
            shdata->idle_cpus -= ncpus_to_borrow;
            process->current_ncpus += ncpus_to_borrow;
            error = DLB_SUCCESS;
        } else {
            // No idle CPUs to borrow
            error = DLB_NOUPDT;
        }

        *new_ncpus = process->current_ncpus;
        idle_cpus = shdata->idle_cpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Borrowing CPUs... New: %d, Idle: %d", *new_ncpus, idle_cpus);
    }

    return error;
}


/*********************************************************************************/
/*  Reset (Lend or Reclaim)                                                      */
/*********************************************************************************/

int shmem_lewi_async__reset(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int *prev_requested) {

    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;

    lewi_process_t *process = get_process(pid);
    if (process == NULL) return DLB_ERR_NOPROC;

    verbose(VB_SHMEM, "Resetting");

    *nreqs = 0;
    *prev_requested = 0;

    int error = DLB_NOUPDT;
    unsigned int idle_cpus;
    shmem_lock(shm_handler);
    {
        if (process->initial_ncpus != process->current_ncpus) {
            error = reset_process(process, requests, nreqs, maxreqs, prev_requested);
        }

        idle_cpus = shdata->idle_cpus;
        *new_ncpus = process->current_ncpus;
    }
    shmem_unlock(shm_handler);

    if (error == DLB_SUCCESS) {
        add_event(IDLE_CPUS_EVENT, idle_cpus);
        verbose(VB_SHMEM, "Using %u CPUs... Idle: %u", *new_ncpus, idle_cpus);
    }

    return error;
}
