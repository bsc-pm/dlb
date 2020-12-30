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

#ifndef SHMEM_PROCINFO_H
#define SHMEM_PROCINFO_H

#include "apis/dlb_types.h"

#include <sys/types.h>
#include <stdbool.h>
#include <sched.h>

/* Init / Register */
int shmem_procinfo__init(pid_t pid, const cpu_set_t *process_mask, cpu_set_t *new_process_mask,
        const char *shmem_key);
int shmem_procinfo_ext__init(const char *shmem_key);
int shmem_procinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags);

/* Finalize / Unregister */
int shmem_procinfo__finalize(pid_t pid, bool return_stolen, const char *shmem_key);
int shmem_procinfo_ext__finalize(void);
int shmem_procinfo_ext__postfinalize(pid_t pid, bool return_stolen);
int shmem_procinfo_ext__recover_stolen_cpus(int pid);

int shmem_procinfo__getprocessmask(pid_t pid, cpu_set_t *mask, dlb_drom_flags_t flags);
int shmem_procinfo__setprocessmask(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags);

/* Generic Getters */
int shmem_procinfo__polldrom(pid_t pid, int *new_cpus, cpu_set_t *new_mask);
int shmem_procinfo__getpidlist(pid_t *pidlist, int *nelems, int max_len);

/* Statistics */
double  shmem_procinfo__getcpuusage(pid_t pid);
double  shmem_procinfo__getcpuavgusage(pid_t pid);
void    shmem_procinfo__getcpuusage_list(double *usagelist, int *nelems, int max_len);
void    shmem_procinfo__getcpuavgusage_list(double *avgusagelist, int *nelems, int max_len);
double  shmem_procinfo__getnodeusage(void);
double  shmem_procinfo__getnodeavgusage(void);
int     shmem_procinfo__getactivecpus(pid_t pid);
void    shmem_procinfo__getactivecpus_list(pid_t *cpuslist, int *nelems, int max_len);
int     shmem_procinfo__getloadavg(pid_t pid, double *load);

int  shmem_procinfo__setcpuusage(pid_t pid, int index, double new_usage);
int  shmem_procinfo__setcpuavgusage(pid_t pid, double new_avg_usage);

int  shmem_procinfo__gettimes(pid_t pid, double *mpi_time, double *useful_time);
void shmem_procinfo__settimes(pid_t pid, double mpi_time, double useful_time);

/* Misc */
void shmem_procinfo__print_info(const char *shmem_key);
bool shmem_procinfo__exists(void);
int  shmem_procinfo__version(void);
size_t shmem_procinfo__size(void);

int auto_resize_start(void);
int auto_resize_end(void);

#endif /* SHMEM_PROCINFO_H */
