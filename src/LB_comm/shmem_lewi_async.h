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

#ifndef SHMEM_LEWI_ASYNC_H
#define SHMEM_LEWI_ASYNC_H

#include "support/queues.h"

bool shmem_lewi_async__exists(void);

int shmem_lewi_async__version(void);

size_t shmem_lewi_async__size(void);

void shmem_lewi_async__remove_requests(pid_t pid);

unsigned int shmem_lewi_async__get_num_requests(pid_t pid);

int shmem_lewi_async__init(pid_t pid, unsigned int ncpus,
        const char *shmem_key, int shm_size_multiplier);

void shmem_lewi_async__finalize(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs);

int shmem_lewi_async__lend_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus, lewi_request_t *requests, unsigned int *nreqs,
        unsigned int maxreqs, unsigned int *prev_requested);

int shmem_lewi_async__lend_keep_cpus(pid_t pid, unsigned int new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int *prev_requested);

int shmem_lewi_async__reclaim(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int prev_requested);

int shmem_lewi_async__acquire_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus, lewi_request_t *requests, unsigned int *nreqs,
        unsigned int maxreqs);

int shmem_lewi_async__borrow_cpus(pid_t pid, unsigned int ncpus,
        unsigned int *new_ncpus);

int shmem_lewi_async__reset(pid_t pid, unsigned int *new_ncpus,
        lewi_request_t *requests, unsigned int *nreqs, unsigned int maxreqs,
        unsigned int *prev_requested);

#endif /* SHMEM_LEWI_ASYNC_H */
