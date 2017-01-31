#ifndef LB_FUNCS_H
#define LB_FUNCS_H

#include <sched.h>
#include "support/types.h"

typedef struct BalancePolicy {
    void (*init) (void);
    void (*finish) (void);
    void (*initIteration) (void);
    void (*finishIteration) (void);
    void (*intoCommunication) (void);
    void (*outOfCommunication) (void);
    void (*intoBlockingCall) (int is_iter, int blocking_mode);
    void (*outOfBlockingCall) (int is_iter);
    void (*updateresources) (int max_resources);
    void (*returnclaimed) (void);
    int (*releasecpu) (int cpu);
    int (*returnclaimedcpu) (int cpu);
    void (*claimcpus) (int cpus);
    void (*acquirecpu) (int cpu);
    void (*acquirecpus) (const cpu_set_t* mask);
    int (*checkCpuAvailability) (int cpu);
    void (*resetDLB) (void);
    void (*disableDLB)(void);
    void (*enableDLB) (void);
    void (*single) (void);
    void (*parallel) (void);
    void (*notifymaskchangeto) (const cpu_set_t*);
} balance_policy_t;

void set_lb_funcs(balance_policy_t *lb_funcs, policy_t policy);

#endif /* LB_FUNCS_H */
