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

/* Status */
static int dummy_Init(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_Finalize(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_Enable(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_Disable(const subprocess_descriptor_t *spd) {return 0;}
/* MPI specific */
static void dummy_IntoCommunication(const subprocess_descriptor_t *spd) {}
static void dummy_OutOfCommunication(const subprocess_descriptor_t *spd) {}
static void dummy_IntoBlockingCall(const subprocess_descriptor_t *spd) {}
static void dummy_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {}
/* Lend */
static int dummy_Lend(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_LendCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Reclaim */
static int dummy_Reclaim(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Acquire */
static int dummy_Acquire(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Return */
static int dummy_ReturnAll(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Misc */
static int dummy_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid) {return 1;}


void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy) {
    // Initialize all fields to dummy by default
    *lb_funcs = (balance_policy_t){
        .init = dummy_Init,
        .finalize = dummy_Finalize,
        .enable = dummy_Enable,
        .disable = dummy_Disable,
        .into_communication = dummy_IntoCommunication,
        .out_of_communication = dummy_OutOfCommunication,
        .into_blocking_call = dummy_IntoBlockingCall,
        .out_of_blocking_call = dummy_OutOfBlockingCall,
        .lend = dummy_Lend,
        .lend_cpu = dummy_LendCpu,
        .lend_cpus = dummy_LendCpus,
        .lend_cpu_mask = dummy_LendCpuMask,
        .reclaim = dummy_Reclaim,
        .reclaim_cpu = dummy_ReclaimCpu,
        .reclaim_cpus = dummy_ReclaimCpus,
        .reclaim_cpu_mask = dummy_ReclaimCpuMask,
        .acquire = dummy_Acquire,
        .acquire_cpu = dummy_AcquireCpu,
        .acquire_cpus = dummy_AcquireCpus,
        .acquire_cpu_mask = dummy_AcquireCpuMask,
        .return_all = dummy_ReturnAll,
        .return_cpu = dummy_ReturnCpu,
        .return_cpu_mask = dummy_ReturnCpuMask,
        .check_cpu_availability = dummy_CheckCpuAvailability,
    };


    // Set according to policy
    switch(policy) {
        case POLICY_NONE:
            break;
        case POLICY_LEWI:
            lb_funcs->init = Lend_light_Init;
            lb_funcs->finalize = Lend_light_Finish;
            lb_funcs->enable = Lend_light_enableDLB;
            lb_funcs->disable = Lend_light_disableDLB;
            lb_funcs->into_communication = Lend_light_IntoCommunication;
            lb_funcs->out_of_communication = Lend_light_OutOfCommunication;
            lb_funcs->into_blocking_call = Lend_light_IntoBlockingCall;
            lb_funcs->out_of_blocking_call = Lend_light_OutOfBlockingCall;
            lb_funcs->acquire_cpus = Lend_light_updateresources;
            break;
        case POLICY_WEIGHT:
            lb_funcs->init = Weight_Init;
            lb_funcs->finalize = Weight_Finish;
            lb_funcs->into_communication = Weight_IntoCommunication;
            lb_funcs->out_of_communication = Weight_OutOfCommunication;
            lb_funcs->into_blocking_call = Weight_IntoBlockingCall;
            lb_funcs->out_of_blocking_call = Weight_OutOfBlockingCall;
            lb_funcs->acquire_cpus = Weight_updateresources;
            break;
        case POLICY_LEWI_MASK:
            lb_funcs->init = lewi_mask_Init;
            lb_funcs->finalize = lewi_mask_Finish;
            lb_funcs->enable = lewi_mask_EnableDLB;
            lb_funcs->disable = lewi_mask_DisableDLB;
            lb_funcs->into_communication = lewi_mask_IntoCommunication;
            lb_funcs->out_of_communication = lewi_mask_OutOfCommunication;
            lb_funcs->into_blocking_call = lewi_mask_IntoBlockingCall;
            lb_funcs->out_of_blocking_call = lewi_mask_OutOfBlockingCall;
            lb_funcs->lend = lewi_mask_Lend;
            lb_funcs->reclaim_cpus = lewi_mask_ReclaimCpus;
            lb_funcs->acquire_cpu = lewi_mask_AcquireCpu;
            lb_funcs->acquire_cpus = lewi_mask_AcquireCpus;
            lb_funcs->acquire_cpu_mask = lewi_mask_AcquireCpuMask;
            lb_funcs->return_all = lewi_mask_ReturnAll;
            break;
        case POLICY_AUTO_LEWI_MASK:
            lb_funcs->init = auto_lewi_mask_Init;
            lb_funcs->finalize = auto_lewi_mask_Finish;
            lb_funcs->enable = auto_lewi_mask_EnableDLB;
            lb_funcs->disable = auto_lewi_mask_DisableDLB;
            lb_funcs->into_communication = auto_lewi_mask_IntoCommunication;
            lb_funcs->out_of_communication = auto_lewi_mask_OutOfCommunication;
            lb_funcs->into_blocking_call = auto_lewi_mask_IntoBlockingCall;
            lb_funcs->out_of_blocking_call = auto_lewi_mask_OutOfBlockingCall;
            lb_funcs->lend = auto_lewi_mask_Lend;
            lb_funcs->lend_cpu = auto_lewi_mask_LendCpu;
            lb_funcs->reclaim_cpus = auto_lewi_mask_ReclaimCpus;
            lb_funcs->acquire_cpu = auto_lewi_mask_AcquireCpu;
            lb_funcs->acquire_cpus = auto_lewi_mask_AcquireCpus;
            lb_funcs->acquire_cpu_mask = auto_lewi_mask_AcquireCpuMask;
            lb_funcs->return_cpu = auto_lewi_mask_ReturnCpu;
            lb_funcs->check_cpu_availability = auto_lewi_mask_CheckCpuAvailability;
            break;
        case POLICY_NEW:
            lb_funcs->init = new_Init;
            lb_funcs->finalize = new_Finish;
            lb_funcs->lend_cpu = new_LendCpu;
            lb_funcs->reclaim = new_Reclaim;
            lb_funcs->reclaim_cpu = new_ReclaimCpu;
            lb_funcs->acquire_cpu = new_AcquireCpu;
            lb_funcs->return_cpu = new_ReturnCpu;
            break;
    }
}
