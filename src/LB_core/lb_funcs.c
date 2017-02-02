

#include "LB_core/lb_funcs.h"
#include "LB_policies/Lend_light.h"
#include "LB_policies/Weight.h"
#include "LB_policies/lewi_mask.h"
#include "LB_policies/autonomous_lewi_mask.h"


/* Status */
static int dummy_init(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_finish(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_enableDLB(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_disableDLB(const subprocess_descriptor_t *spd) {return 0;}
/* MPI specific */
static void dummy_intoCommunication(const subprocess_descriptor_t *spd) {}
static void dummy_outOfCommunication(const subprocess_descriptor_t *spd) {}
static void dummy_intoBlockingCall(const subprocess_descriptor_t *spd) {}
static void dummy_outOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {}
/* Lend */
static int dummy_lend(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_lendCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_lendCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_lendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Reclaim */
static int dummy_reclaim(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_reclaimCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_reclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_reclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Acquire */
static int dummy_acquire(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_acquireCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
static int dummy_acquireCpus(const subprocess_descriptor_t *spd, int ncpus) {return 0;}
static int dummy_acquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    return 0;
}
/* Return */
static int dummy_returnAll(const subprocess_descriptor_t *spd) {return 0;}
static int dummy_returnCpu(const subprocess_descriptor_t *spd, int cpuid) {return 0;}
/* Misc */
static int dummy_checkCpuAvailability(int cpuid) {return 1;}


void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy) {
    // Initialize all fields to dummy by default
    *lb_funcs = (balance_policy_t){
        .init = &dummy_init,
        .finish = &dummy_finish,
        .enableDLB = &dummy_enableDLB,
        .disableDLB = &dummy_disableDLB,
        .intoCommunication = &dummy_intoCommunication,
        .outOfCommunication = &dummy_outOfCommunication,
        .intoBlockingCall = &dummy_intoBlockingCall,
        .outOfBlockingCall = &dummy_outOfBlockingCall,
        .lend = &dummy_lend,
        .lendCpu = &dummy_lendCpu,
        .lendCpus = &dummy_lendCpus,
        .lendCpuMask = &dummy_lendCpuMask,
        .reclaim = &dummy_reclaim,
        .reclaimCpu = &dummy_reclaimCpu,
        .reclaimCpus = &dummy_reclaimCpus,
        .reclaimCpuMask = &dummy_reclaimCpuMask,
        .acquire = &dummy_acquire,
        .acquireCpu = &dummy_acquireCpu,
        .acquireCpus = &dummy_acquireCpus,
        .acquireCpuMask = &dummy_acquireCpuMask,
        .returnAll = &dummy_returnAll,
        .returnCpu = &dummy_returnCpu,
        .checkCpuAvailability = &dummy_checkCpuAvailability,
    };


    // Set according to policy
    switch(policy) {
        case POLICY_NONE:
            break;
        case POLICY_LEWI:
            lb_funcs->init = &Lend_light_Init;
            lb_funcs->finish = &Lend_light_Finish;
            lb_funcs->enableDLB = &Lend_light_enableDLB;
            lb_funcs->disableDLB = &Lend_light_disableDLB;
            lb_funcs->intoCommunication = &Lend_light_IntoCommunication;
            lb_funcs->outOfCommunication = &Lend_light_OutOfCommunication;
            lb_funcs->intoBlockingCall = &Lend_light_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &Lend_light_OutOfBlockingCall;
            lb_funcs->acquireCpus = &Lend_light_updateresources;
            break;
        case POLICY_WEIGHT:
            lb_funcs->init = &Weight_Init;
            lb_funcs->finish = &Weight_Finish;
            lb_funcs->intoCommunication = &Weight_IntoCommunication;
            lb_funcs->outOfCommunication = &Weight_OutOfCommunication;
            lb_funcs->intoBlockingCall = &Weight_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &Weight_OutOfBlockingCall;
            lb_funcs->acquireCpus = &Weight_updateresources;
            break;
        case POLICY_LEWI_MASK:
            lb_funcs->init = &lewi_mask_Init;
            lb_funcs->finish = &lewi_mask_Finish;
            lb_funcs->enableDLB = &lewi_mask_EnableDLB;
            lb_funcs->disableDLB = &lewi_mask_DisableDLB;
            lb_funcs->intoCommunication = &lewi_mask_IntoCommunication;
            lb_funcs->outOfCommunication = &lewi_mask_OutOfCommunication;
            lb_funcs->intoBlockingCall = &lewi_mask_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &lewi_mask_OutOfBlockingCall;
            lb_funcs->lend = &lewi_mask_Lend;
            lb_funcs->reclaimCpus = &lewi_mask_ReclaimCpus;
            lb_funcs->acquireCpu = &lewi_mask_AcquireCpu;
            lb_funcs->acquireCpus = &lewi_mask_AcquireCpus;
            lb_funcs->acquireCpuMask = &lewi_mask_AcquireCpuMask;
            lb_funcs->returnAll = &lewi_mask_ReturnAll;
            break;
        case POLICY_AUTO_LEWI_MASK:
            lb_funcs->init = &auto_lewi_mask_Init;
            lb_funcs->finish = &auto_lewi_mask_Finish;
            lb_funcs->enableDLB = &auto_lewi_mask_EnableDLB;
            lb_funcs->disableDLB = &auto_lewi_mask_DisableDLB;
            lb_funcs->intoCommunication = &auto_lewi_mask_IntoCommunication;
            lb_funcs->outOfCommunication = &auto_lewi_mask_OutOfCommunication;
            lb_funcs->intoBlockingCall = &auto_lewi_mask_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &auto_lewi_mask_OutOfBlockingCall;
            lb_funcs->lend = &auto_lewi_mask_Lend;
            lb_funcs->lendCpu = &auto_lewi_mask_LendCpu;
            lb_funcs->reclaimCpus = &auto_lewi_mask_ReclaimCpus;
            lb_funcs->acquireCpu = &auto_lewi_mask_AcquireCpu;
            lb_funcs->acquireCpus = &auto_lewi_mask_AcquireCpus;
            lb_funcs->acquireCpuMask = &auto_lewi_mask_AcquireCpuMask;
            lb_funcs->returnCpu = &auto_lewi_mask_ReturnCpu;
            lb_funcs->checkCpuAvailability = &auto_lewi_mask_CheckCpuAvailability;
            break;
    }
}
