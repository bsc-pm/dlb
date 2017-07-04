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

#include "LB_core/lb_funcs.h"
#include "LB_policies/Lend_light.h"
#include "LB_policies/Weight.h"
#include "LB_policies/lewi_mask.h"
#include "LB_policies/autonomous_lewi_mask.h"
#include "LB_policies/new.h"
#include "apis/dlb_errors.h"

static int disabled() { return DLB_ERR_NOPOL; }

typedef int (*lb_func_kind1)(const struct SubProcessDescriptor*);
typedef int (*lb_func_kind2)(const struct SubProcessDescriptor*, int);
typedef int (*lb_func_kind3)(const struct SubProcessDescriptor*, const cpu_set_t*);
typedef int (*lb_func_kind4)(struct SubProcessDescriptor*, const cpu_set_t*);

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy) {
    // Initialize all fields to a valid, but disabled, function
    *lb_funcs = (balance_policy_t){
        .init                   = (lb_func_kind1)disabled,
        .finalize               = (lb_func_kind1)disabled,
        .enable                 = (lb_func_kind1)disabled,
        .disable                = (lb_func_kind1)disabled,
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
        .acquire_cpu_mask       = (lb_func_kind3)disabled,
        .borrow                 = (lb_func_kind1)disabled,
        .borrow_cpu             = (lb_func_kind2)disabled,
        .borrow_cpus            = (lb_func_kind2)disabled,
        .borrow_cpu_mask        = (lb_func_kind3)disabled,
        .return_all             = (lb_func_kind1)disabled,
        .return_cpu             = (lb_func_kind2)disabled,
        .return_cpu_mask        = (lb_func_kind3)disabled,
        .check_cpu_availability = (lb_func_kind2)disabled,
        .update_priority_cpus   = (lb_func_kind4)disabled,
    };

    // Set according to policy
    switch(policy) {
        case POLICY_NONE:
            break;
        case POLICY_LEWI:
            lb_funcs->init                   = Lend_light_Init;
            lb_funcs->finalize               = Lend_light_Finish;
            lb_funcs->enable                 = Lend_light_enableDLB;
            lb_funcs->disable                = Lend_light_disableDLB;
            lb_funcs->into_communication     = Lend_light_IntoCommunication;
            lb_funcs->out_of_communication   = Lend_light_OutOfCommunication;
            lb_funcs->into_blocking_call     = Lend_light_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = Lend_light_OutOfBlockingCall;
            lb_funcs->borrow_cpus            = Lend_light_updateresources;
            break;
        case POLICY_WEIGHT:
            lb_funcs->init                   = Weight_Init;
            lb_funcs->finalize               = Weight_Finish;
            lb_funcs->into_communication     = Weight_IntoCommunication;
            lb_funcs->out_of_communication   = Weight_OutOfCommunication;
            lb_funcs->into_blocking_call     = Weight_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = Weight_OutOfBlockingCall;
            lb_funcs->borrow_cpus            = Weight_updateresources;
            break;
        case POLICY_LEWI_MASK:
            lb_funcs->init                   = lewi_mask_Init;
            lb_funcs->finalize               = lewi_mask_Finish;
            lb_funcs->enable                 = lewi_mask_EnableDLB;
            lb_funcs->disable                = lewi_mask_DisableDLB;
            lb_funcs->into_communication     = lewi_mask_IntoCommunication;
            lb_funcs->out_of_communication   = lewi_mask_OutOfCommunication;
            lb_funcs->into_blocking_call     = lewi_mask_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = lewi_mask_OutOfBlockingCall;
            lb_funcs->lend                   = lewi_mask_Lend;
            lb_funcs->reclaim_cpus           = lewi_mask_ReclaimCpus;
            lb_funcs->acquire_cpu            = lewi_mask_AcquireCpu;
            lb_funcs->acquire_cpu_mask       = lewi_mask_AcquireCpuMask;
            lb_funcs->borrow_cpus            = lewi_mask_BorrowCpus;
            lb_funcs->return_all             = lewi_mask_ReturnAll;
            break;
        case POLICY_AUTO_LEWI_MASK:
            lb_funcs->init                   = auto_lewi_mask_Init;
            lb_funcs->finalize               = auto_lewi_mask_Finish;
            lb_funcs->enable                 = auto_lewi_mask_EnableDLB;
            lb_funcs->disable                = auto_lewi_mask_DisableDLB;
            lb_funcs->into_communication     = auto_lewi_mask_IntoCommunication;
            lb_funcs->out_of_communication   = auto_lewi_mask_OutOfCommunication;
            lb_funcs->into_blocking_call     = auto_lewi_mask_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = auto_lewi_mask_OutOfBlockingCall;
            lb_funcs->lend                   = auto_lewi_mask_Lend;
            lb_funcs->lend_cpu               = auto_lewi_mask_LendCpu;
            lb_funcs->reclaim_cpus           = auto_lewi_mask_ReclaimCpus;
            lb_funcs->acquire_cpu            = auto_lewi_mask_AcquireCpu;
            lb_funcs->acquire_cpu_mask       = auto_lewi_mask_AcquireCpuMask;
            lb_funcs->borrow_cpus            = auto_lewi_mask_BorrowCpus;
            lb_funcs->return_cpu             = auto_lewi_mask_ReturnCpu;
            lb_funcs->check_cpu_availability = auto_lewi_mask_CheckCpuAvailability;
            break;
        case POLICY_NEW:
            lb_funcs->init                   = new_Init;
            lb_funcs->finalize               = new_Finish;
            lb_funcs->disable                = new_DisableDLB;
            lb_funcs->into_blocking_call     = new_IntoBlockingCall;
            lb_funcs->out_of_blocking_call   = new_OutOfBlockingCall;
            lb_funcs->lend                   = new_Lend;
            lb_funcs->lend_cpu               = new_LendCpu;
            lb_funcs->lend_cpu_mask          = new_LendCpuMask;
            lb_funcs->reclaim                = new_Reclaim;
            lb_funcs->reclaim_cpu            = new_ReclaimCpu;
            lb_funcs->reclaim_cpus           = new_ReclaimCpus;
            lb_funcs->reclaim_cpu_mask       = new_ReclaimCpuMask;
            lb_funcs->acquire_cpu            = new_AcquireCpu;
            lb_funcs->acquire_cpu_mask       = new_AcquireCpuMask;
            lb_funcs->borrow                 = new_Borrow;
            lb_funcs->borrow_cpu             = new_BorrowCpu;
            lb_funcs->borrow_cpus            = new_BorrowCpus;
            lb_funcs->borrow_cpu_mask        = new_BorrowCpuMask;
            lb_funcs->return_all             = new_Return;
            lb_funcs->return_cpu             = new_ReturnCpu;
            lb_funcs->return_cpu_mask        = new_ReturnCpuMask;
            lb_funcs->check_cpu_availability = new_CheckCpuAvailability;
            lb_funcs->update_priority_cpus   = new_UpdatePriorityCpus;
            break;
    }
}
