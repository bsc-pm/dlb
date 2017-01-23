#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "LB_core/DLB_interface.h"

// Generic dummy callback type
typedef void (*dlb_callback_t)(void);

// Callbacks enum
typedef enum dlb_callbacks_e {
    dlb_callback_set_num_threads  = 1,
    dlb_callback_set_active_mask  = 2,
    dlb_callback_set_process_mask = 3,
    dlb_callback_add_active_mask  = 4,
    dlb_callback_add_process_mask = 5,
    dlb_callback_enable_cpu       = 6,
    dlb_callback_disable_cpu      = 7
} dlb_callbacks_t;

// Callbacks signature
typedef void (*dlb_callback_set_num_threads_t)(int num_threads);
typedef void (*dlb_callback_set_active_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_set_process_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_add_active_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_add_process_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_enable_cpu_t)(int cpuid);
typedef void (*dlb_callback_disable_cpu_t)(int cpuid);


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
