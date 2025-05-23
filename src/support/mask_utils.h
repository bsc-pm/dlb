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

#ifndef MASK_UTILS_H
#define MASK_UTILS_H

#include "support/types.h"
#include <sched.h>

typedef struct mu_cpuset_t {
    cpu_set_t   *set;
    size_t      alloc_size;
    cpuid_t     count;
    cpuid_t     first_cpuid;
    cpuid_t     last_cpuid;
} mu_cpuset_t;

/* System topology */
void mu_init(void);
void mu_finalize(void);
int  mu_get_system_size(void);
void mu_get_system_mask(cpu_set_t *mask);
int  mu_get_system_hwthreads_per_core(void);
bool mu_system_has_smt(void);
int  mu_get_num_cores(void);
int  mu_get_core_id(int cpuid);
const mu_cpuset_t* mu_get_core_mask(int cpuid);
const mu_cpuset_t* mu_get_core_mask_by_coreid(int core_id);
void mu_get_nodes_intersecting_with_cpuset(cpu_set_t *node_set, const cpu_set_t *cpuset);
void mu_get_nodes_subset_of_cpuset(cpu_set_t *node_set, const cpu_set_t *cpuset);
void mu_get_cores_intersecting_with_cpuset(cpu_set_t *core_set, const cpu_set_t *cpuset);
void mu_get_cores_subset_of_cpuset(cpu_set_t *core_set, const cpu_set_t *cpuset);
int  mu_get_cpu_next_core(const cpu_set_t *mask, int prev_cpu);
int  mu_count_cores(const cpu_set_t *mask);
int  mu_get_last_coreid(const cpu_set_t *mask);
int  mu_take_last_coreid(cpu_set_t *mask);
void mu_unset_core(cpu_set_t *mask, int coreid);
void mu_set_core(cpu_set_t *mask, int coreid);


/* Generic mask utilities */
void mu_zero(cpu_set_t *result);
void mu_and(cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2);
void mu_or(cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2);
void mu_xor (cpu_set_t *result, const cpu_set_t *mask1, const cpu_set_t *mask2);
bool mu_equal(const cpu_set_t *mask1, const cpu_set_t *mask2);
bool mu_is_subset(const cpu_set_t *subset, const cpu_set_t *superset);
bool mu_is_superset(const cpu_set_t *superset, const cpu_set_t *subset);
bool mu_is_proper_subset(const cpu_set_t *subset, const cpu_set_t *superset);
bool mu_is_proper_superset(const cpu_set_t *superset, const cpu_set_t *subset);
bool mu_intersects(const cpu_set_t *mask1, const cpu_set_t *mask2);
int  mu_count(const cpu_set_t *mask);
void mu_subtract(cpu_set_t *result, const cpu_set_t *minuend, const cpu_set_t *subtrahend);
int  mu_get_single_cpu(const cpu_set_t *mask);
int  mu_get_first_cpu(const cpu_set_t *mask);
int  mu_get_last_cpu(const cpu_set_t *mask);
int  mu_get_next_cpu(const cpu_set_t *mask, int prev);
int  mu_get_next_unset(const cpu_set_t *mask, int prev);

/* Parsing utilities */
const char* mu_to_str(const cpu_set_t *cpu_set);
void  mu_parse_mask(const char *str, cpu_set_t *mask);
char* mu_parse_to_slurm_format(const cpu_set_t *mask);
bool  mu_equivalent_masks(const char *str1, const char *str2);
void  mu_get_quoted_mask(const cpu_set_t *mask, char *str, size_t namelen);
int   mu_cmp_cpuids_by_ownership(const void *cpuid1, const void *cpuid2, void *mask);
int   mu_cmp_cpuids_by_affinity(const void *cpuid1, const void *cpuid2, void *affinity);

/* Testing */
bool mu_testing_is_initialized(void);
void mu_testing_set_sys_size(int size);
void mu_testing_set_sys(unsigned int num_cpus, unsigned int num_cores,
        unsigned int num_nodes);
void mu_testing_set_sys_masks(const cpu_set_t *sys_mask,
        const cpu_set_t *core_masks, unsigned int num_cores,
        const cpu_set_t *node_masks, unsigned int num_nodes);
void mu_testing_init_nohwloc(void);

#endif /* MASK_UTILS_H */
