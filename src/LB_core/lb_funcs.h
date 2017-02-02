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
    int (*checkCpuAvailability)(int cpuid);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
