/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

/*** queue_t *********************************************************************/

typedef struct queue_t {
    void * queue;
    bool queue_is_internal;

    queue_allocator_t allocator;

    size_t element_size;

    unsigned int elements;
    unsigned int capacity;

    unsigned int head;
    unsigned int tail;
} queue_t;

queue_t* queue__init(size_t element_size, unsigned int capacity, void *ptr,
                     queue_allocator_t allocator) {

    // When the container is managed by the user (ptr != NULL) we can not use the 
    // QUEUE_ALLOC_REALLOC because we would corrupt the users pointer. Hence the queue
    // init fails returning NULL.
    if (allocator == QUEUE_ALLOC_REALLOC && ptr != NULL) {
        return NULL;
    }

    queue_t * queue = malloc(sizeof(queue_t));

    *queue = (const queue_t) {
        .queue = ptr == NULL ? malloc(capacity * element_size) : ptr,
        .queue_is_internal = ptr == NULL,
        .allocator = allocator,
        .element_size = element_size,
        .elements = 0,
        .capacity = capacity,
        .head = 0,
        .tail = 0,
    };

    return queue;
}

void queue__destroy(queue_t *queue) {
    if (queue->queue_is_internal) {
        free(queue->queue);
    }
    free(queue);
}

unsigned int queue__get_size(const queue_t *queue) {
    return queue->elements;
}

unsigned int queue__get_capacity(const queue_t *queue) {
    return queue->capacity;
}

bool queue__is_empty(const queue_t *queue) {
    return queue->elements == 0;
}

bool queue__is_at_capacity(const queue_t *queue) {
    return queue->elements == queue->capacity;
}

static void * queue__get_element_ptr(queue_t *queue, unsigned int idx) {
    return (unsigned char *)queue->queue + (queue->element_size * idx);
}

int queue__take_head(queue_t *queue, void * poped) {
    if (queue->elements == 0) {
        return DLB_NOUPDT;
    }

    queue->elements--;

    // Move head back
    if (queue->head == 0) queue->head = queue->capacity;
    queue->head--;

    // Fetch data from queue
    void * head = queue__get_element_ptr(queue, queue->head);
    if (poped != NULL) {
        memcpy(poped, head, queue->element_size);
    }

    return DLB_SUCCESS;
}

int queue__take_tail(queue_t *queue, void * poped) {
    if (queue->elements == 0) {
        return DLB_NOUPDT;
    }

    queue->elements--;

    // Fetch data form queue
    void * tail = queue__get_element_ptr(queue, queue->tail);
    if (poped != NULL) {
        memcpy(poped, tail, queue->element_size);
    }

    // Move tail forward
    queue->tail++;
    if (queue->tail >= queue->capacity) queue->tail = 0;

    return DLB_SUCCESS;
}

int queue__peek_head(queue_t *queue, void **value_ptr) {
    if (queue->elements == 0) {
        return DLB_NOUPDT;
    }

    // Move head back
    unsigned int peek_head = queue->head;
    if (peek_head == 0) peek_head = queue->elements;
    peek_head--;

    // Fetch pointer to queue storage
    *value_ptr = queue__get_element_ptr(queue, peek_head);

    return DLB_SUCCESS;
}

int queue__peek_tail(queue_t *queue, void **value_ptr) {
    if (queue->elements == 0) {
        return DLB_NOUPDT;
    }

    // Fetch pointer to queue storage
    *value_ptr = queue__get_element_ptr(queue, queue->tail);

    return DLB_SUCCESS;
}

static void queue__realloc(queue_t *queue) {

    unsigned int new_capacity = queue->capacity == 0 ? 1 : queue->capacity * 2;
    void *new_queue = malloc(new_capacity * queue->element_size);
    void *old_queue = queue->queue;

    //ensure(queue->elements != 0 && queue->elements == queue->capacity, "");

    // If the data is in order in the storage then we just make one memcpy.
    if (queue->tail == 0 && queue->head == 0 && queue->elements == queue->capacity) {
        memcpy(new_queue, old_queue, queue->elements * queue->element_size);
    }
    // Otherwise first we copy from head to capacyty and then from 0 to tail.
    else {
        unsigned int n_elements_first_copy = (queue->tail >= queue->head ?
            queue->capacity : queue->head) - queue->tail;

        memcpy(
            new_queue,
            (unsigned char *)old_queue + queue->tail * queue->element_size,
            n_elements_first_copy * queue->element_size
        );

        memcpy(
            (unsigned char *)new_queue + (queue->capacity - queue->tail) * queue->element_size,
            old_queue,
            queue->head * queue->element_size
        );
    }

    queue->queue = new_queue;
    queue->tail  = 0;
    queue->head  = queue->elements;
    queue->capacity = new_capacity;

    free(old_queue);
}

static bool queue__check_capacity(queue_t *queue) {
    if (queue__is_at_capacity(queue)) {
        switch (queue->allocator) {
        case QUEUE_ALLOC_FIXED:
            return false;
        case QUEUE_ALLOC_REALLOC:
            queue__realloc(queue);
            break;
        case QUEUE_ALLOC_OVERWRITE:
            break;
        default: return false;
        }
    }

    return true;
}

void * queue__push_head(queue_t *queue, const void * new) {

    if (! queue__check_capacity(queue)) return NULL;

    void * head = queue__get_element_ptr(queue, queue->head);
    memcpy(head, new, queue->element_size);

    unsigned int old_head = queue->head;

    // Move head forward
    queue->head++;
    if (queue->head >= queue->capacity) {
        queue->head = 0;
    }

    queue->elements++;

    return queue__get_element_ptr(queue, old_head);
}

void * queue__push_tail(queue_t *queue, const void * new) {

    if (! queue__check_capacity(queue)) return NULL;

    // Move tail back
    if (queue->tail == 0) queue->tail = queue->capacity;
    queue->tail--;

    queue->elements++;

    void * tail = queue__get_element_ptr(queue, queue->tail);
    memcpy(tail, new, queue->element_size);

    return queue__get_element_ptr(queue, queue->tail);
}

/*********************************************************************************/

/*** queue_t generic functions for iterators *************************************/

typedef struct queue_iter_t {
    void* (*next)(void*);
} queue_iter_t;

void queue_iter__foreach(void *iter, void (*fn)(void*, void*), void* args) {
    queue_iter_t *it = iter;
    void * i = NULL;
    while ( (i = it->next(iter)) != NULL) {
        fn(args, i);
    }
}

void* queue_iter__get_nth(void *iter, unsigned int n) {
    queue_iter_t *it = iter;
    void * i = NULL;

    unsigned int count = 0;
    while ( (i = it->next(iter)) != NULL) {
        if (count == n) {
            return i;
        }

        count++;
    }

    return NULL;
}

/*** queue_iter_head2tail_t implementation ***************************************/

static void * queue_iter_head2tail__next(void *iter) {
    queue_iter_head2tail_t *it = iter;

    if (it->done || it->queue->elements == 0) {
        return NULL;
    }

    if (it->curr <= 0) {
        it->curr = queue__get_capacity(it->queue);
    }
    it->curr--;

    if (it->curr == it->tail) {
        it->done = true;
    }

    return queue__get_element_ptr(it->queue, it->curr);
}

queue_iter_head2tail_t queue__into_head2tail_iter(queue_t * queue) {
    return (queue_iter_head2tail_t) {
        .next = queue_iter_head2tail__next,
        .queue = queue,
        .done = false,
        .curr = queue->head,
        .tail = queue->tail,
    };
}

/*********************************************************************************/
