#ifndef LB_FUNCS_H
#define LB_FUNCS_H

#include <sched.h>
#include "support/types.h"

// FIXME: forward declaration of an already exising typedef
typedef struct SubProcessDescriptor subprocess_descriptor_t;

typedef struct BalancePolicy {
    void (*init)(const subprocess_descriptor_t *spd);
    void (*finish)(const subprocess_descriptor_t *spd);
    void (*enableDLB)(const subprocess_descriptor_t *spd);
    void (*disableDLB)(const subprocess_descriptor_t *spd);
    void (*intoCommunication)(const subprocess_descriptor_t *spd);
    void (*outOfCommunication)(const subprocess_descriptor_t *spd);
    void (*intoBlockingCall)(const subprocess_descriptor_t *spd);
    void (*outOfBlockingCall)(const subprocess_descriptor_t *spd, int is_iter);
    void (*updateresources)(const subprocess_descriptor_t *spd, int max_resources);
    void (*returnclaimed)(const subprocess_descriptor_t *spd);
    int (*releasecpu)(const subprocess_descriptor_t *spd, int cpu);
    int (*returnclaimedcpu)(const subprocess_descriptor_t *spd, int cpu);
    void (*claimcpus)(const subprocess_descriptor_t *spd, int cpus);
    void (*acquirecpu)(const subprocess_descriptor_t *spd, int cpu);
    void (*acquirecpus)(const subprocess_descriptor_t *spd, const cpu_set_t* mask);
    int (*checkCpuAvailability) (int cpu);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
