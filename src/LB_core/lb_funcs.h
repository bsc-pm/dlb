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
    int (*finish)(const subprocess_descriptor_t *spd);
    int (*enableDLB)(const subprocess_descriptor_t *spd);
    int (*disableDLB)(const subprocess_descriptor_t *spd);
    /* MPI specific */
    void (*intoCommunication)(const subprocess_descriptor_t *spd);
    void (*outOfCommunication)(const subprocess_descriptor_t *spd);
    void (*intoBlockingCall)(const subprocess_descriptor_t *spd);
    void (*outOfBlockingCall)(const subprocess_descriptor_t *spd, int is_iter);
    /* Lend */
    int (*lend)(const subprocess_descriptor_t *spd);
    int (*lendCpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*lendCpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*lendCpuMask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Reclaim */
    int (*reclaim)(const subprocess_descriptor_t *spd);
    int (*reclaimCpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*reclaimCpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*reclaimCpuMask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Acquire */
    int (*acquire)(const subprocess_descriptor_t *spd);
    int (*acquireCpu)(const subprocess_descriptor_t *spd, int cpuid);
    int (*acquireCpus)(const subprocess_descriptor_t *spd, int ncpus);
    int (*acquireCpuMask)(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
    /* Return */
    int (*returnAll)(const subprocess_descriptor_t *spd);
    int (*returnCpu)(const subprocess_descriptor_t *spd, int cpuid);
    /* Misc */
    int (*checkCpuAvailability)(const subprocess_descriptor_t *spd, int cpuid);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
