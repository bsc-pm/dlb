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

#include <sched.h>
#include "support/types.h"

// FIXME: forward declaration of an already exising typedef
typedef struct SubProcessDescriptor subprocess_descriptor_t;

typedef struct BalancePolicy {
    /* Status */
    int (*init)(const subprocess_descriptor_t *spd);
    int (*finalize)(const subprocess_descriptor_t *spd);
    int (*enable)(const subprocess_descriptor_t *spd);
    int (*disable)(const subprocess_descriptor_t *spd);
    /* MPI specific */
    void (*into_communication)(const subprocess_descriptor_t *spd);
    void (*out_of_communication)(const subprocess_descriptor_t *spd);
    void (*into_blocking_call)(const subprocess_descriptor_t *spd);
    void (*out_of_blocking_call)(const subprocess_descriptor_t *spd, int is_iter);
    /* Lend */
    int (*lend)(const subprocess_descriptor_t *spd);
    int (*lend_cpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*lend_cpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*lend_cpu_mask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Reclaim */
    int (*reclaim)(const subprocess_descriptor_t *spd);
    int (*reclaim_cpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*reclaim_cpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*reclaim_cpu_mask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Acquire */
    int (*acquire)(const subprocess_descriptor_t *spd);
    int (*acquire_cpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*acquire_cpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*acquire_cpu_mask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Return */
    int (*return_all)(const subprocess_descriptor_t *spd);
    int (*return_cpu)(const subprocess_descriptor_t *spd, int cpuid);
    /* Misc */
    int (*check_cpu_availability)(const subprocess_descriptor_t *spd, int cpuid);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
