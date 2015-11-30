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

#ifndef MASK_UTILS_H
#define MASK_UTILS_H

#define _GNU_SOURCE
#include <sched.h>

typedef enum {
    MU_ANY_BIT,
    MU_ALL_BITS
} mu_opt_t;

void mu_init(void);
void mu_finalize(void);
int  mu_get_system_size(void);
void mu_get_affinity_mask(cpu_set_t *affinity_set, const cpu_set_t *child_set, mu_opt_t condition);

const char* mu_to_str(const cpu_set_t *cpu_set);
void mu_parse_mask(const char *str, cpu_set_t *mask);

#endif /* MASK_UTILS_H */
