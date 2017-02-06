/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

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
