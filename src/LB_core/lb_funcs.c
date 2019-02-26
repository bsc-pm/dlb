/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "LB_core/lb_funcs.h"
#include "LB_policies/lewi.h"
#include "LB_policies/lewi_mask.h"
#include "apis/dlb_errors.h"

static int disabled() { return DLB_ERR_NOPOL; }
static int dummy(struct SubProcessDescriptor *spd) { return DLB_SUCCESS; }

typedef int (*lb_func_kind1)(const struct SubProcessDescriptor*);
typedef int (*lb_func_kind2)(const struct SubProcessDescriptor*, int);
typedef int (*lb_func_kind3)(const struct SubProcessDescriptor*, const cpu_set_t*);

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy) {
    // Initialize all fields to a valid, but disabled, function
    *lb_funcs = (balance_policy_t){
        .init                   = dummy,
        .finalize               = dummy,
        .enable                 = (lb_func_kind1)disabled,
        .disable                = (lb_func_kind1)disabled,
        .set_max_parallelism    = (lb_func_kind2)disabled,
        .unset_max_parallelism  = (lb_func_kind1)disabled,
        .into_communication     = (lb_func_kind1)disabled,
        .out_of_communication   = (lb_func_kind1)disabled,
        .into_blocking_call     = (lb_func_kind1)disabled,
        .out_of_blocking_call   = (lb_func_kind2)disabled,
        .lend                   = (lb_func_kind1)disabled,
        .lend_cpu               = (lb_func_kind2)disabled,
        .lend_cpus              = (lb_func_kind2)disabled,
        .lend_cpu_mask          = (lb_func_kind3)disabled,
        .reclaim                = (lb_func_kind1)disabled,
        .reclaim_cpu            = (lb_func_kind2)disabled,
        .reclaim_cpus           = (lb_func_kind2)disabled,
        .reclaim_cpu_mask       = (lb_func_kind3)disabled,
        .acquire_cpu            = (lb_func_kind2)disabled,
        .acquire_cpus           = (lb_func_kind2)disabled,
        .acquire_cpu_mask       = (lb_func_kind3)disabled,
        .borrow                 = (lb_func_kind1)disabled,
        .borrow_cpu             = (lb_func_kind2)disabled,
        .borrow_cpus            = (lb_func_kind2)disabled,
        .borrow_cpu_mask        = (lb_func_kind3)disabled,
        .return_all             = (lb_func_kind1)disabled,
        .return_cpu             = (lb_func_kind2)disabled,
        .return_cpu_mask        = (lb_func_kind3)disabled,
        .check_cpu_availability = (lb_func_kind2)disabled,
        .update_ownership_info  = (lb_func_kind3)disabled,
    };

    // Set according to policy
    switch(policy) {
        case POLICY_NONE:
            break;
        case POLICY_LEWI:
            lb_funcs->init                   = lewi_Init;
            lb_funcs->finalize               = lewi_Finalize;
            lb_funcs->enable                 = lewi_EnableDLB;
            lb_funcs->disable                = lewi_DisableDLB;
            lb_funcs->set_max_parallelism    = lewi_SetMaxParallelism;
            lb_funcs->unset_max_parallelism  = lewi_UnsetMaxParallelism;
            lb_funcs->into_communication     = lewi_IntoCommunication;
            lb_funcs->out_of_communication   = lewi_OutOfCommunication;
            lb_funcs->into_blocking_call     = lewi_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = lewi_OutOfBlockingCall;
            lb_funcs->lend                   = lewi_Lend;
            lb_funcs->reclaim                = lewi_Reclaim;
            lb_funcs->borrow                 = lewi_Borrow;
            lb_funcs->borrow_cpus            = lewi_BorrowCpus;
            break;
        case POLICY_LEWI_MASK:
            lb_funcs->init                   = lewi_mask_Init;
            lb_funcs->finalize               = lewi_mask_Finalize;
            lb_funcs->enable                 = lewi_mask_EnableDLB;
            lb_funcs->disable                = lewi_mask_DisableDLB;
            lb_funcs->set_max_parallelism    = lewi_mask_SetMaxParallelism;
            lb_funcs->unset_max_parallelism  = lewi_mask_UnsetMaxParallelism;
            lb_funcs->into_blocking_call     = lewi_mask_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = lewi_mask_OutOfBlockingCall;
            lb_funcs->lend                   = lewi_mask_Lend;
            lb_funcs->lend_cpu               = lewi_mask_LendCpu;
            lb_funcs->lend_cpu_mask          = lewi_mask_LendCpuMask;
            lb_funcs->reclaim                = lewi_mask_Reclaim;
            lb_funcs->reclaim_cpu            = lewi_mask_ReclaimCpu;
            lb_funcs->reclaim_cpus           = lewi_mask_ReclaimCpus;
            lb_funcs->reclaim_cpu_mask       = lewi_mask_ReclaimCpuMask;
            lb_funcs->acquire_cpu            = lewi_mask_AcquireCpu;
            lb_funcs->acquire_cpus           = lewi_mask_AcquireCpus;
            lb_funcs->acquire_cpu_mask       = lewi_mask_AcquireCpuMask;
            lb_funcs->borrow                 = lewi_mask_Borrow;
            lb_funcs->borrow_cpu             = lewi_mask_BorrowCpu;
            lb_funcs->borrow_cpus            = lewi_mask_BorrowCpus;
            lb_funcs->borrow_cpu_mask        = lewi_mask_BorrowCpuMask;
            lb_funcs->return_all             = lewi_mask_Return;
            lb_funcs->return_cpu             = lewi_mask_ReturnCpu;
            lb_funcs->return_cpu_mask        = lewi_mask_ReturnCpuMask;
            lb_funcs->check_cpu_availability = lewi_mask_CheckCpuAvailability;
            lb_funcs->update_ownership_info  = lewi_mask_UpdateOwnershipInfo;
            break;
    }
}
