#ifndef SPD_H
#define SPD_H

#include "LB_numThreads/numThreads.h"
#include "support/options.h"

/* Sub-process Descriptor */

typedef struct {
    cpu_set_t process_mask;
    options_t options;
    pm_interface_t pm;
} spd_t;

extern spd_t global_spd;

#endif /* SPD_H */
