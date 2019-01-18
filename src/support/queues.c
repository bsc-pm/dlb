/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "support/queues.h"

#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <string.h>

enum { NOBODY = 0 };

/*** queue_proc_reqs_t ***********************************************************/

void queue_proc_reqs_init(queue_proc_reqs_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

unsigned int queue_proc_reqs_size(queue_proc_reqs_t *queue) {
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
    unsigned int i;
    for (i=queue->tail; i!=queue->head; i=(i+1)%QUEUE_PROC_REQS_SIZE) {
        if (queue->queue[i].pid == pid) {
            queue->queue[i].pid = NOBODY;
            queue->queue[i].howmany = 0;
        }
        /* Advance tail if it points to invalid elements */
        if (queue->queue[i].pid == NOBODY
                && i == queue->tail) {
            queue->tail = (queue->tail+1) % QUEUE_PROC_REQS_SIZE;
        }
    }
}

int queue_proc_reqs_push(queue_proc_reqs_t *queue, pid_t pid, unsigned int howmany) {
    /* Remove entry if value is reseted to 0 */
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

    /* If the queue is full, retun error code */
    unsigned int next_head = (queue->head + 1) % QUEUE_PROC_REQS_SIZE;
    if (__builtin_expect((next_head == queue->tail), 0)) {
        return DLB_ERR_REQST;
    }

    /* Otherwise, push new entry */
    queue->queue[queue->head].pid = pid;
    queue->queue[queue->head].howmany = howmany;
    queue->head = next_head;

    return DLB_NOTED;
}

void queue_proc_reqs_pop(queue_proc_reqs_t *queue, pid_t *pid) {
    *pid = NOBODY;
    while(queue->head != queue->tail && *pid == NOBODY) {
        process_request_t *request = &queue->queue[queue->tail];
        if (request->pid != NOBODY) {
            /* request is valid */
            *pid = request->pid;
            if (--request->howmany == 0) {
                request->pid = NOBODY;
                queue->tail = (queue->tail + 1) % QUEUE_PROC_REQS_SIZE;
            }
        } else {
            /* request is not valid, advance tail */
            queue->tail = (queue->tail + 1) % QUEUE_PROC_REQS_SIZE;
        }
    }
}

/*********************************************************************************/

/*** queue_pids_t ****************************************************************/

void queue_pids_init(queue_pids_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

unsigned int queue_pids_size(queue_pids_t *queue) {
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
