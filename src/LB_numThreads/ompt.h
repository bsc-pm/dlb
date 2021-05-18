/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef OMPT_H
#define OMPT_H

#include <stdint.h>

/* OMPT related types and signatures compliant with the OpenMP API Specification 5.0 */

/*********************************************************************************/
/*  enumerations                                                                 */
/*********************************************************************************/
typedef enum ompt_callbacks_t {
    ompt_callback_thread_begin          = 1,
    ompt_callback_thread_end            = 2,
    ompt_callback_parallel_begin        = 3,
    ompt_callback_parallel_end          = 4,
    ompt_callback_task_create           = 5,
    ompt_callback_task_schedule         = 6,
    ompt_callback_implicit_task         = 7,
    ompt_callback_target                = 8,
    ompt_callback_target_data_op        = 9,
    ompt_callback_target_submit         = 10,
    ompt_callback_control_tool          = 11,
    ompt_callback_device_initialize     = 12,
    ompt_callback_device_finalize       = 13,
    ompt_callback_device_load           = 14,
    ompt_callback_device_unload         = 15,
    ompt_callback_sync_region_wait      = 16,
    ompt_callback_mutex_released        = 17,
    ompt_callback_dependences           = 18,
    ompt_callback_task_dependence       = 19,
    ompt_callback_work                  = 20,
    ompt_callback_master                = 21,
    ompt_callback_target_map            = 22,
    ompt_callback_sync_region           = 23,
    ompt_callback_lock_init             = 24,
    ompt_callback_lock_destroy          = 25,
    ompt_callback_mutex_acquire         = 26,
    ompt_callback_mutex_acquired        = 27,
    ompt_callback_nest_lock             = 28,
    ompt_callback_flush                 = 29,
    ompt_callback_cancel                = 30,
    ompt_callback_reduction             = 31,
    ompt_callback_dispatch              = 32
} ompt_callbacks_t;

typedef enum ompt_thread_t {
    ompt_thread_initial     = 1,
    ompt_thread_worker      = 2,
    ompt_thread_other       = 3,
    ompt_thread_unknown     = 4
} ompt_thread_t;

typedef enum ompt_set_result_t {
    ompt_set_error              = 0,
    ompt_set_never              = 1,
    ompt_set_impossible         = 2,
    ompt_set_sometimes          = 3,
    ompt_set_sometimes_paired   = 4,
    ompt_set_always             = 5
} ompt_set_result_t;

typedef enum ompt_scope_endpoint_t {
    ompt_scope_begin    = 1,
    ompt_scope_end      = 2
} ompt_scope_endpoint_t;

typedef enum ompt_parallel_flag_t {
    ompt_parallel_invoker_program   = 0x00000001,
    ompt_parallel_invoker_runtime   = 0x00000002,
    ompt_parallel_league            = 0x40000000,
    ompt_parallel_team              = 0x80000000
} ompt_parallel_flag_t;


/*********************************************************************************/
/*  data types                                                                   */
/*********************************************************************************/
typedef union ompt_data_t {
    uint64_t value;
    void *ptr;
} ompt_data_t;

typedef struct ompt_frame_t {
    ompt_data_t exit_frame;
    ompt_data_t enter_frame;
    int exit_frame_flags;
    int enter_frame_flags;
} ompt_frame_t;


/*********************************************************************************/
/*  callback signatures                                                          */
/*********************************************************************************/
typedef void (*ompt_callback_t)(void);

typedef int (*ompt_set_callback_t)(
    ompt_callbacks_t event,
    ompt_callback_t callback
);

typedef void (*ompt_callback_thread_begin_t) (
    ompt_thread_t thread_type,
    ompt_data_t *thread_data
);

typedef void (*ompt_callback_thread_end_t) (
    ompt_data_t *thread_data
);

typedef void (*ompt_callback_parallel_begin_t) (
    ompt_data_t *encountering_task_data,
    const ompt_frame_t *encountering_task_frame,
    ompt_data_t *parallel_data,
    unsigned int requested_parallelism,
    int flags,
    const void *codeptr_ra
);

typedef void (*ompt_callback_parallel_end_t) (
    ompt_data_t *parallel_data,
    ompt_data_t *encountering_task_data,
    int flags,
    const void *codeptr_ra
);

typedef void (*ompt_callback_implicit_task_t) (
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int actual_parallelism,
    unsigned int index,
    int flags
);


/*********************************************************************************/
/*  start_tool types                                                             */
/*********************************************************************************/
typedef void (*ompt_interface_fn_t)(void);

typedef ompt_interface_fn_t (*ompt_function_lookup_t)(const char *interface_function_name);

typedef int (*ompt_initialize_t)(ompt_function_lookup_t lookup, int initial_device_num,
        ompt_data_t *tool_data);

typedef void (*ompt_finalize_t)(ompt_data_t *tool_data);

typedef struct ompt_start_tool_result_t {
    ompt_initialize_t initialize;
    ompt_finalize_t finalize;
    ompt_data_t tool_data;
} ompt_start_tool_result_t;

#endif /* OMPT_H */
