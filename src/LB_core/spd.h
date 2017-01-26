#ifndef SPD_H
#define SPD_H

#include "support/options.h"

/* Sub-process Descriptor */

typedef struct {
    int a;
    options_t options;
} spd_t;

extern spd_t global_spd;

#endif /* SPD_H */
