/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef SHMEM_PROCINFO_H
#define SHMEM_PROCINFO_H

#include <sys/types.h>
#include <stdbool.h>
#include <sched.h>

int shmem_procinfo__init(pid_t pid, const cpu_set_t *process_mask, cpu_set_t *new_process_mask,
        const char *shmem_key);
int shmem_procinfo_ext__init(const char *shmem_key);
int shmem_procinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, int steal);

int shmem_procinfo__finalize(pid_t pid);
int shmem_procinfo_ext__finalize(void);
int shmem_procinfo_ext__postfinalize(pid_t pid, int return_stolen);

int shmem_procinfo__getprocessmask(pid_t pid, cpu_set_t *mask);
int shmem_procinfo__polldrom(pid_t pid, int *new_cpus, cpu_set_t *new_mask);

void shmem_procinfo_ext__getpidlist(int *pidlist, int *nelems, int max_len);
int shmem_procinfo_ext__getprocessmask(int pid, cpu_set_t *mask);
int shmem_procinfo_ext__setprocessmask(int pid, const cpu_set_t *mask);
double shmem_procinfo_ext__getcpuusage(int pid);
double shmem_procinfo_ext__getcpuavgusage(int pid);
void shmem_procinfo_ext__getcpuusage_list(double *usagelist, int *nelems, int max_len);
void shmem_procinfo_ext__getcpuavgusage_list(double *avgusagelist, int *nelems, int max_len);
double shmem_procinfo_ext__getnodeusage(void);
double shmem_procinfo_ext__getnodeavgusage(void);
int shmem_procinfo_ext__getactivecpus(int pid);
void shmem_procinfo_ext__getactivecpus_list(int *cpuslist, int *nelems, int max_len);
int shmem_procinfo_ext__getloadavg(int pid, double *load);
void shmem_procinfo_ext__print_info(bool statistics);
bool shmem_procinfo__exists(void);


#endif /* SHMEM_PROCINFO_H */
