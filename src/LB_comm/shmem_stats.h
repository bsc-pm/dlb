/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#ifndef SHMEM_STATS_H
#define SHMEM_STATS_H

typedef enum {
    STATS_IDLE = 0,
    STATS_OWNED,
    STATS_GUESTED
} stats_state_t;

void shmem_stats__init(void);
void shmem_stats__finalize(void);
void shmem_stats__update(void);
void shmem_stats__update_cpu(int cpu, stats_state_t new_state);
void shmem_stats_ext__init( void );
void shmem_stats_ext__finalize(void);
int shmem_stats_ext__getnumcpus(void);
void shmem_stats_ext__getpidlist(int *pidlist,int *nelems,int max_len);
double shmem_stats_ext__getcpuusage(int pid);
double shmem_stats_ext__getcpuavgusage(int pid);
void shmem_stats_ext__getcpuusage_list(double *usagelist,int *nelems,int max_len);
void shmem_stats_ext__getcpuavgusage_list(double *avgusagelist,int *nelems,int max_len);
double shmem_stats_ext__getnodeusage(void);
double shmem_stats_ext__getnodeavgusage(void);
int shmem_stats_ext__getactivecpus(int pid);
void shmem_stats_ext__getactivecpus_list(int *cpuslist,int *nelems,int max_len);
int shmem_stats_ext__getloadavg(int pid,double *load);
float shmem_stats_ext__getcpustate(int cpu, stats_state_t state);
void shmem_stats_ext__print_info(void);

#endif /* SHMEM_STATS_H */
