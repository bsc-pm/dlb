#ifndef SPD_H
#define SPD_H

// Old gcc's need sys/types for pid_t definition
#include <sys/types.h>

#include "LB_core/lb_funcs.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/types.h"

/* Sub-process Descriptor */

typedef struct {
    cpu_set_t process_mask;
    cpu_set_t active_mask;
    options_t options;
    pm_interface_t pm;
    balance_policy_t lb_funcs;
    pid_t process_id;
} spd_t;

extern spd_t global_spd;

#endif /* SPD_H */
