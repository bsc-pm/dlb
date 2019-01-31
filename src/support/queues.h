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

#ifndef QUEUES_H
#define QUEUES_H

#include <sched.h>
#include <unistd.h>
#include <stdbool.h>

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

#endif /* QUEUES_H */
