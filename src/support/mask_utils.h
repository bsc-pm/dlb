/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

void mu_init(void);
void mu_finalize(void);
int  mu_get_system_size(void);
void mu_get_system_mask(cpu_set_t *mask);
void mu_get_parents_covering_cpuset(cpu_set_t *parent_set, const cpu_set_t *cpuset);
void mu_get_parents_inside_cpuset(cpu_set_t *parent_set, const cpu_set_t *cpuset);
bool mu_is_subset(const cpu_set_t *subset, const cpu_set_t *superset);
bool mu_is_superset(const cpu_set_t *superset, const cpu_set_t *subset);
bool mu_is_proper_subset(const cpu_set_t *subset, const cpu_set_t *superset);
bool mu_is_proper_superset(const cpu_set_t *superset, const cpu_set_t *subset);
bool mu_intersects(const cpu_set_t *mask1, const cpu_set_t *mask2);
void mu_substract(cpu_set_t *result, const cpu_set_t *minuend, const cpu_set_t *substrahend);

const char* mu_to_str(const cpu_set_t *cpu_set);
void mu_parse_mask(const char *str, cpu_set_t *mask);
int mu_cmp_cpuids_by_ownership(const void *cpuid1, const void *cpuid2, void *mask);
int mu_cmp_cpuids_by_topology(const void *cpuid1, const void *cpuid2, void *topology);

void mu_testing_set_sys_size(int size);

#endif /* MASK_UTILS_H */
