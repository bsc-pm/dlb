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

#ifndef SHMEM_TALP_H
#define SHMEM_TALP_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct talp_region_list_t {
    pid_t pid;
    int region_id;
    int_least64_t mpi_time;
    int_least64_t useful_time;
    float avg_cpus;
} talp_region_list_t;

/* Init */
int shmem_talp__init(const char *shmem_key, int shmem_size_multiplier);
int shmem_talp_ext__init(const char *shmem_key, int shmem_size_multiplier);

/* Finalize */
int shmem_talp__finalize(pid_t pid);
int shmem_talp_ext__finalize(void);

/* Register */
int shmem_talp__register(pid_t pid, float avg_cpus, const char *name, int *node_shared_id);

/* Getters */
int shmem_talp__get_pidlist(pid_t *pidlist, int *nelems, int max_len);
int shmem_talp__get_region(talp_region_list_t *region, pid_t pid, const char *name);
int shmem_talp__get_regionlist(talp_region_list_t *region_list, int *nelems,
        int max_len, const char *name);
int shmem_talp__get_times(int region_id, int64_t *mpi_time, int64_t *useful_time);

/* Setters */
int shmem_talp__set_times(int region_id, int64_t mpi_time, int64_t useful_time);
int shmem_talp__set_avg_cpus(int region_id, float avg_cpus);

/* Misc */
void shmem_talp__print_info(const char *shmem_key, int shmem_size_multiplier);
bool shmem_talp__exists(void);
bool shmem_talp__initialized(void);
int  shmem_talp__version(void);
size_t shmem_talp__size(void);
int shmem_talp__get_max_regions(void);
int shmem_talp__get_num_regions(void);

#endif /* SHMEM_TALP_H */
