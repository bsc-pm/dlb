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

#ifndef QUEUES_H
#define QUEUES_H

#include <sched.h>
#include <unistd.h>
#include <stdbool.h>


/*** queue_lewi_reqs_t ***********************************************************/
/* queue_lewi_reqs_t is a packed set of pairs (<pid>,howmany), where <pid> is
 * unique key; 'head' points to the next empty position and is always strictly
 * less than QUEUE_LEWI_REQS_SIZE */

enum { QUEUE_LEWI_REQS_SIZE = 256 };

typedef struct {
    pid_t        pid;
    unsigned int howmany;
} lewi_request_t;

typedef struct {
    lewi_request_t  queue[QUEUE_LEWI_REQS_SIZE];
    unsigned int    head;
} queue_lewi_reqs_t;

void queue_lewi_reqs_init(queue_lewi_reqs_t *queue);
unsigned int queue_lewi_reqs_size(const queue_lewi_reqs_t *queue);
unsigned int queue_lewi_reqs_remove(queue_lewi_reqs_t *queue, pid_t pid);
int queue_lewi_reqs_push(queue_lewi_reqs_t *queue, pid_t pid,
        unsigned int howmany);
unsigned int queue_lewi_reqs_pop_ncpus(queue_lewi_reqs_t *queue, unsigned int ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs);
unsigned int queue_lewi_reqs_get(queue_lewi_reqs_t *queue, pid_t pid);


/*** queue_proc_reqs_t ***********************************************************/
enum { QUEUE_PROC_REQS_SIZE = 4096 };

typedef struct {
    pid_t        pid;
    unsigned int howmany;
    cpu_set_t    allowed;
} process_request_t;

typedef struct {
    process_request_t queue[QUEUE_PROC_REQS_SIZE];
    unsigned int      head;
    unsigned int      tail;
} queue_proc_reqs_t;

void queue_proc_reqs_init(queue_proc_reqs_t *queue);
unsigned int queue_proc_reqs_size(const queue_proc_reqs_t *queue);
process_request_t* queue_proc_reqs_front(queue_proc_reqs_t *queue);
process_request_t* queue_proc_reqs_back(queue_proc_reqs_t *queue);
void queue_proc_reqs_remove(queue_proc_reqs_t *queue, pid_t pid);
int  queue_proc_reqs_push(queue_proc_reqs_t *queue, pid_t pid,
        unsigned int howmany, const cpu_set_t *allowed);
void queue_proc_reqs_pop(queue_proc_reqs_t *queue, process_request_t *request);
void queue_proc_reqs_get(queue_proc_reqs_t *queue, pid_t *pid, int cpuid);


/*** queue_pids_t ****************************************************************/
enum { QUEUE_PIDS_SIZE = 8 };

typedef struct {
    pid_t        queue[QUEUE_PIDS_SIZE];
    unsigned int head;
    unsigned int tail;
} queue_pids_t;

void queue_pids_init(queue_pids_t *queue);
unsigned int queue_pids_size(const queue_pids_t *queue);
void queue_pids_remove(queue_pids_t *queue, pid_t pid);
int  queue_pids_push(queue_pids_t *queue, pid_t pid);
void queue_pids_pop(queue_pids_t *queue, pid_t *pid);

/*** queue_t *********************************************************************/

typedef enum queue_allocator_e {
    QUEUE_ALLOC_FIXED,
    QUEUE_ALLOC_OVERWRITE,
    QUEUE_ALLOC_REALLOC,
} queue_allocator_t;

typedef struct queue_t queue_t;

/**
 * Initializes a queue with a pre-allocated storage.
 *
 *  - element_size: the size of the type to be stored.
 *  - capacity: the number of elements that fit in the queue.
 *  - ptr: the pointer to the memory allocated for storage.
 *  - allocator: chooses what to do when the queue is full.
 *
 *  Notes:
 *   - Set ptr to null if you want the allocation to be done automatically.
 *   - When ptr is NOT null the QUEUE_ALLOC_REALLOC is not a valid allocator.
 *
 *  Returns NULL if it fails.
 */
queue_t* queue__init(size_t element_size, unsigned int capacity, void *ptr, queue_allocator_t allocator);

/**
 * Destroys a queue and frees the storage.
 *
 *  - queue: the queue to destroy.
 */
void queue__destroy(queue_t *queue);

/**
 * Returns the number of elements avaliable in the queue.
 *
 *  - queue: the queue where the operation is performed.
 */
unsigned int queue__get_size(const queue_t *queue);

/**
 * Returns the maximum number of elements the queue can fit.
 *
 *  - queue: the queue where the operation is performed.
 */
unsigned int queue__get_capacity(const queue_t *queue);

/**
 * Returns true if the queue does not contain any element.
 */
bool queue__is_empty(const queue_t *queue);

/** 
 * Returns true if the number of elements is equal to the capacity of the
 * queue.
 *
 * When the queue is at capacity it means that if more data is puhsed to the
 * queue without removing this action will cause an error, overwrite a value
 * from the other end, or reallocate the storage (depending on the choosen
 * allocation policy).
 */
bool queue__is_at_capacity(const queue_t *queue);

/**
 * Takes the head off of the queue and places it to poped.
 *
 *  - queue: the queue where the operation is performed.
 *  - poped: the pointer of where to place the head.
 *
 *  Returns DLB_NOUPDT if the queue is empty, and DLB_SUCCESS otherwise.
 */
int queue__take_head(queue_t *queue, void *value);

/**
 * Takes the tail off of the queue and places it to poped.
 *
 *  - queue: the queue where the operation is performed.
 *  - poped: the pointer of where to place the tail.
 *
 *  Returns DLB_NOUPDT if the queue is empty, and DLB_SUCCESS otherwise.
 */
int queue__take_tail(queue_t *queue, void *value);

/**
 * Peek the head and return its value without removing it from the queue.
 *
 *  - queue: the queue where the operation is performed.
 *  - poped: the pointer of where to copy the reference to head.
 *
 *  Returns DLB_NOUPDT if the queue is empty, and DLB_SUCCESS otherwise.
 */
int queue__peek_head(queue_t *queue, void **value_ptr);

/**
 * Peek the tail and return its value without removing it from the queue.
 *
 *  - queue: the queue where the operation is performed.
 *  - poped: the pointer of where to copy the reference to tail.
 *
 *  Returns DLB_NOUPDT if the queue is empty, and DLB_SUCCESS otherwise.
 */
int queue__peek_tail(queue_t *queue, void **value_ptr);

/** 
 * Pushes the new value to the queue on the head, and overwrites the tail if
 * the queue was at capacity.
 *
 *  - queue: the queue where the operation is performed.
 *  - new: the pointer to the value to push.
 *
 *  Returns NULL if the push fails.
 */
void * queue__push_head(queue_t *queue, const void * new);

/** 
 * Pushes the new value to the queue on the head, and overwrites the tail if
 * the queue was at capacity.
 *
 *  - queue: the queue where the operation is performed.
 *  - new: the pointer to the value to push.
 *
 *  Returns NULL if the push fails.
 */
void * queue__push_tail(queue_t *queue, const void * new);

/*** queue_t iterators ***********************************************************/

/** 
 * Executes the fn function for all the values of iter as it is iterated by
 * its specific implementation.
 *
 *  - iter: the pointer to the iterator
 *  - fn: the function to run for all elements it receives as arguments args
 *        and the current element of the iterator
 *  - args: a pointer to a user defined variable
 */
void queue_iter__foreach(void *iter, void (*fn)(void*, void*), void* args);

/** 
 * Returns a pointer to the nth element in the iterator.
 *
 *  - iter: the pointer to the iterator
 *  - n: the number of elements to skip before returning
 *
 * Returns NULL if not enough elements are avaliable.
 */
void* queue_iter__get_nth(void *iter, unsigned int n);

typedef struct queue_iter_head2tail_t {
    void* (*next)(void*);
    queue_t * queue;
    bool done;
    unsigned int curr;
    unsigned int tail;
} queue_iter_head2tail_t;

/**
 * Creates a queue_iter_head2tail_t iterator from a queue_t.
 *
 *  - queue: the queue from which the iterator will be created.
 */
queue_iter_head2tail_t queue__into_head2tail_iter(queue_t * queue);

#endif /* QUEUES_H */
