

#include "LB_core/lb_funcs.h"
#include "LB_policies/Lend_light.h"
#include "LB_policies/Weight.h"
#include "LB_policies/JustProf.h"
#include "LB_policies/Lewi_map.h"
#include "LB_policies/lewi_mask.h"
#include "LB_policies/autonomous_lewi_mask.h"
#include "LB_policies/RaL.h"
#include "LB_policies/PERaL.h"

// Initialize lb_funcs to dummy functions
static void dummy_init(const cpu_set_t *process_mask) {}
static void dummy_finish(void) {}
static void dummy_initIteration(void) {}
static void dummy_finishIteration(void) {}
static void dummy_intoCommunication(void) {}
static void dummy_outOfCommunication(void) {}
static void dummy_intoBlockingCall(int is_iter, int blocking_mode) {}
static void dummy_outOfBlockingCall(int is_iter) {}
static void dummy_updateresources(int max_resources) {}
static void dummy_returnclaimed(void) {}
static int dummy_releasecpu(int cpu) {return 0;}
static int dummy_returnclaimedcpu(int cpu) {return 0;}
static void dummy_claimcpus(int cpus) {}
static void dummy_acquirecpu(int cpu) {}
static void dummy_acquirecpus(const cpu_set_t* mask) {}
static int dummy_checkCpuAvailability(int cpu) {return 1;}
static void dummy_resetDLB(void) {}
static void dummy_disableDLB(void) {}
static void dummy_enableDLB(void) {}
static void dummy_single(void) {}
static void dummy_parallel(void) {}

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy) {
    // Initialize all fields to dummy by default
    *lb_funcs = (balance_policy_t){
        .init = &dummy_init,
        .finish = &dummy_finish,
        .initIteration = &dummy_initIteration,
        .finishIteration = &dummy_finishIteration,
        .intoCommunication = &dummy_intoCommunication,
        .outOfCommunication = &dummy_outOfCommunication,
        .intoBlockingCall = &dummy_intoBlockingCall,
        .outOfBlockingCall = &dummy_outOfBlockingCall,
        .updateresources = &dummy_updateresources,
        .returnclaimed = &dummy_returnclaimed,
        .releasecpu = &dummy_releasecpu,
        .returnclaimedcpu = &dummy_returnclaimedcpu,
        .claimcpus = &dummy_claimcpus,
        .acquirecpu = &dummy_acquirecpu,
        .acquirecpus = &dummy_acquirecpus,
        .checkCpuAvailability = &dummy_checkCpuAvailability,
        .resetDLB = &dummy_resetDLB,
        .disableDLB = &dummy_disableDLB,
        .enableDLB = &dummy_enableDLB,
        .single = &dummy_single,
        .parallel = &dummy_parallel,
    };


    // Set according to policy
    switch(policy) {
        case POLICY_NONE:
            break;
        case POLICY_LEWI:
            lb_funcs->init = &Lend_light_Init;
            lb_funcs->finish = &Lend_light_Finish;
            lb_funcs->intoCommunication = &Lend_light_IntoCommunication;
            lb_funcs->outOfCommunication = &Lend_light_OutOfCommunication;
            lb_funcs->intoBlockingCall = &Lend_light_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &Lend_light_OutOfBlockingCall;
            lb_funcs->updateresources = &Lend_light_updateresources;
            lb_funcs->resetDLB = &Lend_light_resetDLB;
            lb_funcs->disableDLB = &Lend_light_disableDLB;
            lb_funcs->enableDLB = &Lend_light_enableDLB;
            lb_funcs->single = &Lend_light_single;
            lb_funcs->parallel = &Lend_light_parallel;
            break;
        case POLICY_WEIGHT:
            lb_funcs->init = &Weight_Init;
            lb_funcs->finish = &Weight_Finish;
            lb_funcs->intoCommunication = &Weight_IntoCommunication;
            lb_funcs->outOfCommunication = &Weight_OutOfCommunication;
            lb_funcs->intoBlockingCall = &Weight_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &Weight_OutOfBlockingCall;
            lb_funcs->updateresources = &Weight_updateresources;
            break;
        case POLICY_LEWI_MASK:
            lb_funcs->init = &lewi_mask_Init;
            lb_funcs->finish = &lewi_mask_Finish;
            lb_funcs->intoCommunication = &lewi_mask_IntoCommunication;
            lb_funcs->outOfCommunication = &lewi_mask_OutOfCommunication;
            lb_funcs->intoBlockingCall = &lewi_mask_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &lewi_mask_OutOfBlockingCall;
            lb_funcs->updateresources = &lewi_mask_UpdateResources;
            lb_funcs->returnclaimed = &lewi_mask_ReturnClaimedCpus;
            lb_funcs->claimcpus = &lewi_mask_ClaimCpus;
            lb_funcs->resetDLB  = &lewi_mask_resetDLB;
            lb_funcs->acquirecpu = &lewi_mask_acquireCpu;
            lb_funcs->acquirecpus = &lewi_mask_acquireCpus;
            lb_funcs->disableDLB = &lewi_mask_disableDLB;
            lb_funcs->enableDLB = &lewi_mask_enableDLB;
            lb_funcs->single = &lewi_mask_single;
            lb_funcs->parallel = &lewi_mask_parallel;
            break;
        case POLICY_AUTO_LEWI_MASK:
            lb_funcs->init = &auto_lewi_mask_Init;
            lb_funcs->finish = &auto_lewi_mask_Finish;
            lb_funcs->intoCommunication = &auto_lewi_mask_IntoCommunication;
            lb_funcs->outOfCommunication = &auto_lewi_mask_OutOfCommunication;
            lb_funcs->intoBlockingCall = &auto_lewi_mask_IntoBlockingCall;
            lb_funcs->outOfBlockingCall = &auto_lewi_mask_OutOfBlockingCall;
            lb_funcs->updateresources = &auto_lewi_mask_UpdateResources;
            //lb_funcs->returnclaimed = &auto_lewi_mask_ReturnClaimedCpus;
            lb_funcs->releasecpu = &auto_lewi_mask_ReleaseCpu;
            lb_funcs->returnclaimedcpu = &auto_lewi_mask_ReturnCpuIfClaimed;
            lb_funcs->claimcpus = &auto_lewi_mask_ClaimCpus;
            lb_funcs->checkCpuAvailability = &auto_lewi_mask_CheckCpuAvailability;
            lb_funcs->resetDLB = &auto_lewi_mask_resetDLB;
            lb_funcs->acquirecpu = &auto_lewi_mask_acquireCpu;
            lb_funcs->acquirecpus = &auto_lewi_mask_acquireCpus;
            lb_funcs->disableDLB = &auto_lewi_mask_disableDLB;
            lb_funcs->enableDLB = &auto_lewi_mask_enableDLB;
            lb_funcs->single = &auto_lewi_mask_single;
            lb_funcs->parallel = &auto_lewi_mask_parallel;
            break;
    }
}

// Things to do outside if this function is separated from kernel:
// Print policy name:
