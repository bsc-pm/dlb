#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "LB_core/DLB_interface.h"
#include "LB_core/dlb_types.h"

/*
 * Getters should be removed, and so this file
 */

// Generic dummy callback type
typedef void (*dlb_getter_t)(void);

// Callbacks enum
typedef enum dlb_getters_e {
    dlb_getter_get_thread_num   = 1,
    dlb_getter_get_num_threads  = 2,
    dlb_getter_get_active_mask  = 3,
    dlb_getter_get_process_mask = 4
} dlb_getters_t;

// Callbacks signature
typedef int (*dlb_getter_get_thread_num_t)(void);
typedef int (*dlb_getter_get_num_threads_t)(void);
typedef void (*dlb_getter_get_active_mask_t)(dlb_cpu_set_t mask);
typedef void (*dlb_getter_get_process_mask_t)(dlb_cpu_set_t mask);


#endif /* CALLBACKS_H */
