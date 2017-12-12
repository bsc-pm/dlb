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

#ifndef LB_FUNCS_H
#define LB_FUNCS_H

#include "support/types.h"

#include <sched.h>

struct SubProcessDescriptor;

typedef struct BalancePolicy {
    /* Status */
    int (*init)(struct SubProcessDescriptor *spd);
    int (*finalize)(struct SubProcessDescriptor *spd);
    int (*enable)(const struct SubProcessDescriptor *spd);
    int (*disable)(const struct SubProcessDescriptor *spd);
    int (*set_max_parallelism)(const struct SubProcessDescriptor *spd, int max);
    /* MPI specific */
    int (*into_communication)(const struct SubProcessDescriptor *spd);
    int (*out_of_communication)(const struct SubProcessDescriptor *spd);
    int (*into_blocking_call)(const struct SubProcessDescriptor *spd);
    int (*out_of_blocking_call)(const struct SubProcessDescriptor *spd, int is_iter);
    /* Lend */
    int (*lend)(const struct SubProcessDescriptor *spd);
    int (*lend_cpu)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*lend_cpus)(const struct SubProcessDescriptor *spd, int ncpus);
    int (*lend_cpu_mask)(const struct SubProcessDescriptor *spd, const cpu_set_t *mask);
    /* Reclaim */
    int (*reclaim)(const struct SubProcessDescriptor *spd);
    int (*reclaim_cpu)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*reclaim_cpus)(const struct SubProcessDescriptor *spd, int ncpus);
    int (*reclaim_cpu_mask)(const struct SubProcessDescriptor *spd, const cpu_set_t *mask);
    /* Acquire */
    int (*acquire_cpu)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*acquire_cpus)(const struct SubProcessDescriptor *spd, int ncpus);
    int (*acquire_cpu_mask)(const struct SubProcessDescriptor *spd, const cpu_set_t *mask);
    /* Borrow */
    int (*borrow)(const struct SubProcessDescriptor *spd);
    int (*borrow_cpu)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*borrow_cpus)(const struct SubProcessDescriptor *spd, int ncpus);
    int (*borrow_cpu_mask)(const struct SubProcessDescriptor *spd, const cpu_set_t *mask);
    /* Return */
    int (*return_all)(const struct SubProcessDescriptor *spd);
    int (*return_cpu)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*return_cpu_mask)(const struct SubProcessDescriptor *spd, const cpu_set_t *mask);
    /* Misc */
    int (*check_cpu_availability)(const struct SubProcessDescriptor *spd, int cpuid);
    int (*update_ownership_info)(const struct SubProcessDescriptor *spd,
            const cpu_set_t *process_mask);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
