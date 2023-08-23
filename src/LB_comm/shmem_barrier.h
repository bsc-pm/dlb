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

#ifndef SHMEM_BARRIER_H
#define SHMEM_BARRIER_H

#include <stddef.h>
#include <stdbool.h>

enum { BARRIER_NAME_MAX = 32 };

typedef struct barrier_t barrier_t;

void shmem_barrier__init(const char *shmem_key);
void shmem_barrier__finalize(const char *shmem_key);
int  shmem_barrier__get_system_id(void);
int  shmem_barrier__get_max_barriers(void);
barrier_t* shmem_barrier__find(const char *barrier_name);
barrier_t* shmem_barrier__register(const char *barrier_name, bool lewi);
int  shmem_barrier__attach(barrier_t *barrier);
int  shmem_barrier__detach(barrier_t *barrier);
void shmem_barrier__barrier(barrier_t *barrier);

void shmem_barrier__print_info(const char *shmem_key);
bool shmem_barrier__exists(void);
int shmem_barrier__version(void);
size_t shmem_barrier__size(void);

#endif /* SHMEM_BARRIER_H */
