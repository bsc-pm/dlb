/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#ifndef NUMTHREADS_H
#define NUMTHREADS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include "LB_core/callbacks.h"

typedef struct pm_interface {
    /* Callbacks list */
    dlb_callback_set_num_threads_t dlb_callback_set_num_threads_ptr;
    dlb_callback_set_active_mask_t dlb_callback_set_active_mask_ptr;
    dlb_callback_set_process_mask_t dlb_callback_set_process_mask_ptr;
    dlb_callback_add_active_mask_t dlb_callback_add_active_mask_ptr;
    dlb_callback_add_process_mask_t dlb_callback_add_process_mask_ptr;
    dlb_callback_enable_cpu_t dlb_callback_enable_cpu_ptr;
    dlb_callback_disable_cpu_t dlb_callback_disable_cpu_ptr;

    /* Getters list */
    dlb_getter_get_num_threads_t dlb_getter_get_num_threads_ptr;
    dlb_getter_get_active_mask_t dlb_getter_get_active_mask_ptr;
    dlb_getter_get_process_mask_t dlb_getter_get_process_mask_ptr;
} pm_interface_t;

void pm_init(pm_interface_t *pm);
int pm_callback_set(pm_interface_t *pm, dlb_callbacks_t which, dlb_callback_t callback);
int pm_callback_get(const pm_interface_t *pm, dlb_callbacks_t which, dlb_callback_t callback);
int pm_getter_set(pm_interface_t *pm, dlb_getters_t which, dlb_getter_t getter);
int pm_getter_get(const pm_interface_t *pm, dlb_getters_t which, dlb_getter_t getter);

int update_threads(const pm_interface_t *pm, int threads);
int set_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set);
int set_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set);
int add_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set);
int add_process_mask(const pm_interface_t *pm, const cpu_set_t *cpu_set);
int enable_cpu(const pm_interface_t *pm, int cpuid);
int disable_cpu(const pm_interface_t *pm, int cpuid);

int get_num_threads(const pm_interface_t *pm);
int get_mask(const pm_interface_t *pm, cpu_set_t *cpu_set);
int get_process_mask(const pm_interface_t *pm, cpu_set_t *cpu_set);

#endif //NUMTHREADS_H
