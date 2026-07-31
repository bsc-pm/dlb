/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#ifndef DLB_OPENACC_ABI_H
#define DLB_OPENACC_ABI_H

/* These definitions mirror a subset of OpenACC 3.1: 5. Profiling Interface.
 * Although unlikely, these definitions are prefixed with dlb_ to prevent
 * redefinition errors in case the real <acc_prof.h> or <openacc.h> are visible. */

typedef enum dlb_acc_event_t {
    dlb_acc_ev_none = 0,
    dlb_acc_ev_device_init_start = 1,
    dlb_acc_ev_device_init_end = 2,
    dlb_acc_ev_device_shutdown_start = 3,
    dlb_acc_ev_device_shutdown_end = 4,
    dlb_acc_ev_runtime_shutdown = 5,
    dlb_acc_ev_create = 6,
    dlb_acc_ev_delete = 7,
    dlb_acc_ev_alloc = 8,
    dlb_acc_ev_free = 9,
    dlb_acc_ev_enter_data_start = 10,
    dlb_acc_ev_enter_data_end = 11,
    dlb_acc_ev_exit_data_start = 12,
    dlb_acc_ev_exit_data_end = 13,
    dlb_acc_ev_update_start = 14,
    dlb_acc_ev_update_end = 15,
    dlb_acc_ev_compute_construct_start = 16,
    dlb_acc_ev_compute_construct_end = 17,
    dlb_acc_ev_enqueue_launch_start = 18,
    dlb_acc_ev_enqueue_launch_end = 19,
    dlb_acc_ev_enqueue_upload_start = 20,
    dlb_acc_ev_enqueue_upload_end = 21,
    dlb_acc_ev_enqueue_download_start = 22,
    dlb_acc_ev_enqueue_download_end = 23,
    dlb_acc_ev_wait_start = 24,
    dlb_acc_ev_wait_end = 25,
} dlb_acc_event_t;

typedef enum dlb_acc_register_t {
    dlb_acc_reg = 0,
    dlb_acc_toggle = 1,
    dlb_acc_toggle_per_thread = 2,
} dlb_acc_register_t;

/* `acc_device_t` is a vendor-specific type definition.
 * According to the standard, it's an enumeration type supporting all the
 * device types.  However, while GCC and Nvidia define it as a regular enum (32
 * bit), Cray does it with a typedef int64_t (64 bit). */
typedef enum dlb_acc_device_t {
    dlb_acc_device_none = 0,
    dlb_acc_device_default = 1,
} dlb_acc_device_t;

typedef struct dlb_acc_prof_info {
    dlb_acc_event_t event_type;
    int valid_bytes;
    int version;
    /* WARNING: do not access any field below this, as the offset is
     * not guaranteed at compile time.
     * If, at some time, we need some of them, we'll need to define two
     * structs, one with acc_device_t_32 and another with acc_device_t_64
     * and cast the provided pointer to the appropriate depending if we
     * can detect if we are using Cray implementation. */
    dlb_acc_device_t device_type;
    int device_number;
    int thread_id;
    ssize_t async;
    ssize_t async_queue;
    const char *src_file;
    const char *func_name;
    int line_no, end_line_no;
    int func_line_no, func_end_line_no;
} dlb_acc_prof_info;

typedef struct dlb_acc_event_info dlb_acc_event_info;
typedef struct dlb_acc_api_info dlb_acc_api_info;

typedef void (*dlb_acc_prof_callback)(dlb_acc_prof_info*, dlb_acc_event_info*, dlb_acc_api_info*);

#endif /* DLB_OPENACC_ABI_H */
