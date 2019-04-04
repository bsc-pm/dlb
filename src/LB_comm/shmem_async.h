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

#ifndef SHMEM_ASYNC_H
#define SHMEM_ASYNC_H

#include <sched.h>
#include <sys/types.h>

struct pm_interface;

int shmem_async_init(pid_t pid, const struct pm_interface *pm,
        const cpu_set_t *process_mask, const char *shmem_key);
int shmem_async_finalize(pid_t pid);

void shmem_async_enable_cpu(pid_t pid, int cpuid);
void shmem_async_disable_cpu(pid_t pid, int cpuid);

int shmem_async__version(void);
size_t shmem_async__size(void);

#endif /* SHMEM_ASYNC_H */
