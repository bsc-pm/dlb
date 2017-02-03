#ifndef DLB_TYPES_H
#define DLB_TYPES_H

// Opaque types
typedef void* dlb_handler_t;
typedef void* dlb_cpu_set_t;
typedef const void* const_dlb_cpu_set_t;

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

// Callback signatures
typedef void (*dlb_callback_set_num_threads_t)(int num_threads);
typedef void (*dlb_callback_set_active_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_set_process_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_add_active_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_add_process_mask_t)(const_dlb_cpu_set_t mask);
typedef void (*dlb_callback_enable_cpu_t)(int cpuid);
typedef void (*dlb_callback_disable_cpu_t)(int cpuid);

#endif /* DLB_TYPES_H */
