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

#include "support/queues.h"

#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <string.h>

enum { NOBODY = 0 };

/*** queue_lewi_reqs_t ***********************************************************/

void queue_lewi_reqs_init(queue_lewi_reqs_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

unsigned int queue_lewi_reqs_size(const queue_lewi_reqs_t *queue) {
    return queue->head;
}

/* Remove entry with given <pid> and pack list, return previous value if exists */
unsigned int queue_lewi_reqs_remove(queue_lewi_reqs_t *queue, pid_t pid) {

    if (unlikely(pid == 0)) return 0;
    if (queue->head == 0) return 0;

    /* The queue does not need to be ordered. Find element to remove and swap
     * last position */
    for (unsigned int i=0; i < queue->head; ++i) {
        lewi_request_t *request = &queue->queue[i];
        if (request->pid == pid) {
            // return value
            int howmany = request->howmany;

            if (i+1 < queue->head) {
                // this is not the last element, copy last one here
                *request = queue->queue[queue->head-1];
            }
            --queue->head;

            return howmany;
        }
    }

    return 0;
}

/* Push entry with given <pid>, add value if already present */
int queue_lewi_reqs_push(queue_lewi_reqs_t *queue, pid_t pid, unsigned int howmany) {

    if (unlikely(pid == 0)) return DLB_NOUPDT;

    /* Remove entry if value is reset to 0 */
    if (howmany == 0) {
        queue_lewi_reqs_remove(queue, pid);
        return DLB_SUCCESS;
    }

    /* Find entry, if exists */
    unsigned int i;
    for (i=0; i<queue->head; ++i) {
        if (queue->queue[i].pid == pid) {
            /* Found, just update value */
            queue->queue[i].howmany += howmany;
            return DLB_SUCCESS;
        }
    }

    /* No existing entry and 'head' points to the last position, which is reserved */
    if (queue->head == QUEUE_LEWI_REQS_SIZE-1) {
        return DLB_ERR_REQST;
    }

    /* 'i' (and 'head') points the new spot. Add entry. */
    queue->queue[i] = (const lewi_request_t) {
        .pid = pid,
        .howmany = howmany,
    };
    ++queue->head;
    return DLB_SUCCESS;
}

/* Remove entries with no values (howmany == 0) */
static void queue_lewi_reqs_pack(queue_lewi_reqs_t *queue) {
    unsigned int i, i_write;
    for (i=0, i_write=0; i<queue->head; ++i) {
        if (queue->queue[i].howmany > 0) {
            if (i != i_write) {
                queue->queue[i_write] = queue->queue[i];
            }
            ++i_write;
        }
    }
    queue->head = i_write;
}

/* Sort in descending order by number of requests */
static int sort_queue_lewi_reqs(const void *elem1, const void *elem2) {
    return ((lewi_request_t*)elem2)->howmany - ((lewi_request_t*)elem1)->howmany;
}

/* Pop ncpus from the queue in an equal distribution, return array of accepted requests
 * by argument, and number of CPUs not assigned */
unsigned int queue_lewi_reqs_pop_ncpus(queue_lewi_reqs_t *queue, unsigned int ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs) {

    unsigned int queue_size = queue_lewi_reqs_size(queue);
    if (queue_size == 0) {
        *nreqs = 0;
        return ncpus;
    }

    /* For evenly distribute CPU removal, we need to sort the queue by number
     * of requests per process */
    qsort(queue->queue, queue_size, sizeof(queue->queue[0]), sort_queue_lewi_reqs);

    ensure(queue->queue[queue->head-1].howmany > 0, "Empty spots in the queue");

    unsigned int i, j;
    for (i = queue->head, j = 0; i-- > 0 && ncpus > 0 && j < maxreqs; ) {
        lewi_request_t *request = &queue->queue[i];
        /* For every iteration, we compute the CPUs to be popped by computing
         * the minimum between an even distribution and the current request
         * value. */
        unsigned int ncpus_popped = min_uint(ncpus/(i+1), request->howmany);
        if (ncpus_popped > 0) {
            /* Assign number of CPUs to output array */
            requests[j++] = (const lewi_request_t) {
                .pid = request->pid,
                .howmany = ncpus_popped,
            };

            /* Remove the CPUs from the queue */
            request->howmany -= ncpus_popped;

            /* Update number of CPUs to be popped */
            ncpus -= ncpus_popped;
        }
    }

    /* Update output array size */
    *nreqs = j;

    /* Pack queue, remove elements if howmany reached 0 */
    queue_lewi_reqs_pack(queue);

    return ncpus;
}

/* Return the value of requests of the given pid */
unsigned int queue_lewi_reqs_get(queue_lewi_reqs_t *queue, pid_t pid) {

    if (unlikely(pid == 0)) return 0;

    for (unsigned int i = 0; i < queue->head; ++i) {
        if (queue->queue[i].pid == pid) {
            return queue->queue[i].howmany;
        }
    }

    return 0;
}


/*** queue_proc_reqs_t ***********************************************************/

void queue_proc_reqs_init(queue_proc_reqs_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

unsigned int queue_proc_reqs_size(const queue_proc_reqs_t *queue) {
    return queue->head >= queue->tail
        ? queue->head - queue->tail
        : QUEUE_PROC_REQS_SIZE - queue->tail + queue->head;
}

process_request_t* queue_proc_reqs_front(queue_proc_reqs_t *queue) {
    return &queue->queue[queue->tail];
}

process_request_t* queue_proc_reqs_back(queue_proc_reqs_t *queue) {
    unsigned int prev_head = queue->head > 0 ? queue->head-1 : QUEUE_PROC_REQS_SIZE-1;
    return &queue->queue[prev_head];
}

void queue_proc_reqs_remove(queue_proc_reqs_t *queue, pid_t pid) {
    if (unlikely(!pid)) return;

    unsigned int i;
    for (i=queue->tail; i!=queue->head; i=(i+1)%QUEUE_PROC_REQS_SIZE) {
        if (queue->queue[i].pid == pid) {
            /* Clear request */
            memset(&queue->queue[i], 0, sizeof(process_request_t));
        }
        /* Advance tail if it points to invalid elements */
        if (queue->queue[i].pid == NOBODY
                && i == queue->tail) {
            queue->tail = (queue->tail+1) % QUEUE_PROC_REQS_SIZE;
        }
    }
}

int queue_proc_reqs_push(queue_proc_reqs_t *queue, pid_t pid,
        unsigned int howmany, const cpu_set_t *allowed) {
    /* Remove entry if value is reset to 0 */
    if (howmany == 0) {
        queue_proc_reqs_remove(queue, pid);
        return DLB_SUCCESS;
    }

    /* If the last request belongs to the same pid, just increment value */
    process_request_t *last_req = queue_proc_reqs_back(queue);
    if (last_req->pid == pid) {
        last_req->howmany += howmany;
        return DLB_NOTED;
    }

    /* If the queue is full, return error code */
    unsigned int next_head = (queue->head + 1) % QUEUE_PROC_REQS_SIZE;
    if (__builtin_expect((next_head == queue->tail), 0)) {
        return DLB_ERR_REQST;
    }

    /* Otherwise, push new entry */
    queue->queue[queue->head].pid = pid;
    queue->queue[queue->head].howmany = howmany;
    if (allowed) {
        memcpy(&queue->queue[queue->head].allowed, allowed, sizeof(*allowed));
    } else {
        memset(&queue->queue[queue->head].allowed,0xff, sizeof(*allowed));
    }
    queue->head = next_head;

    return DLB_NOTED;
}

void queue_proc_reqs_pop(queue_proc_reqs_t *queue, process_request_t *request) {
    /* If front is valid, extract request */
    process_request_t *front = queue_proc_reqs_front(queue);
    if (front->pid != NOBODY) {
        if (request != NULL) {
            /* Copy request if argument is valid */
            memcpy(request, front, sizeof(process_request_t));
        }
        /* Clear request */
        memset(front, 0, sizeof(process_request_t));
    }

    /* Advance tail as long as it points to invalid elements */
    while(queue->head != queue->tail
            && queue->queue[queue->tail].pid == NOBODY) {
        queue->tail = (queue->tail + 1) % QUEUE_PROC_REQS_SIZE;
    }
}

void queue_proc_reqs_get(queue_proc_reqs_t *queue, pid_t *pid, int cpuid) {
    *pid = NOBODY;
    unsigned int pointer = queue->tail;
    while(pointer != queue->head && *pid == NOBODY) {
        process_request_t *request = &queue->queue[pointer];
        if (request->pid != NOBODY
                && CPU_ISSET(cpuid, &request->allowed)) {
            /* request is valid */
            *pid = request->pid;
            if (--request->howmany == 0) {
                request->pid = NOBODY;
                CPU_ZERO(&request->allowed);
            }
        }

        /* Advance tail if possible */
        if (pointer == queue->tail
                && request->pid == NOBODY) {
            queue->tail = (queue->tail + 1) % QUEUE_PROC_REQS_SIZE;
        }

        /* Advance pointer */
        pointer = (pointer + 1) % QUEUE_PROC_REQS_SIZE;
    }

    /* Advance tail as long as it points to invalid elements */
    while(queue->head != queue->tail
            && queue->queue[queue->tail].pid == NOBODY) {
        queue->tail = (queue->tail + 1) % QUEUE_PROC_REQS_SIZE;
    }
}

/*********************************************************************************/

/*** queue_pids_t ****************************************************************/

void queue_pids_init(queue_pids_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

unsigned int queue_pids_size(const queue_pids_t *queue) {
    return queue->head >= queue->tail
        ? queue->head - queue->tail
        : QUEUE_PIDS_SIZE - queue->tail + queue->head;
}

void queue_pids_remove(queue_pids_t *queue, pid_t pid) {
    unsigned int i;
    for (i=queue->tail; i!=queue->head; i=(i+1)%QUEUE_PIDS_SIZE) {
        if (queue->queue[i] == pid) {
            queue->queue[i] = NOBODY;
        }
        /* Advance tail if it points to invalid elements */
        if (queue->queue[i] == NOBODY
                && i == queue->tail) {
            queue->tail = (queue->tail+1) % QUEUE_PIDS_SIZE;
        }
    }
}

int queue_pids_push(queue_pids_t *queue, pid_t pid) {
    unsigned int next_head = (queue->head + 1) % QUEUE_PIDS_SIZE;
    if (__builtin_expect((next_head == queue->tail), 0)) {
        return DLB_ERR_REQST;
    }
    queue->queue[queue->head] = pid;
    queue->head = next_head;
    return DLB_NOTED;
}

void queue_pids_pop(queue_pids_t *queue, pid_t *pid) {
    *pid = NOBODY;
    while(queue->head != queue->tail && *pid == NOBODY) {
        *pid = queue->queue[queue->tail];
        queue->queue[queue->tail] = NOBODY;
        queue->tail = (queue->tail + 1) % QUEUE_PIDS_SIZE;
    }
}
/*********************************************************************************/
