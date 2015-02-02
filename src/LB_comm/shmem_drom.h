/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#ifndef SHMEM_DROM_H
#define SHMEM_DROM_H

#define _GNU_SOURCE
#include <sched.h>

void shmem_drom__init(void);
void shmem_drom__finalize(void);
void shmem_drom__update(void);

void shmem_drom_ext__init(void);
void shmem_drom_ext__finalize(void);
int shmem_drom_ext__getnumcpus(void);
void shmem_drom_ext__getpidlist(int *pidlist, int *nelems, int max_len);
void shmem_drom_ext__getprocessmask(int pid, cpu_set_t *mask);
void shmem_drom_ext__setprocessmask(int pid, const cpu_set_t *mask);
void shmem_drom_ext__printinfo(void);

#endif /* SHMEM_DROM_H */
