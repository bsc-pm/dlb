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

#ifndef DLB_KERNEL_SP_H
#define DLB_KERNEL_SP_H

#include "LB_core/DLB_kernel_sp.h"

#include "LB_core/spd.h"
#include "apis/dlb_types.h"
#include "support/options.h"

#include <sched.h>
#include <stdbool.h>

/* Status */
subprocess_descriptor_t* Initialize_sp(int ncpus, const cpu_set_t *mask, const char *lb_args);
int Finish_sp(subprocess_descriptor_t *spd);
int set_dlb_enabled_sp(subprocess_descriptor_t *spd, bool enabled);
int set_max_parallelism_sp(subprocess_descriptor_t *spd, int max);

/* Callbacks */
int callback_set_sp(subprocess_descriptor_t *spd, dlb_callbacks_t which, dlb_callback_t callback);
int callback_get_sp(subprocess_descriptor_t *spd, dlb_callbacks_t which, dlb_callback_t *callback);

/* Lend */
int lend_sp(subprocess_descriptor_t *spd);
int lend_cpu_sp(subprocess_descriptor_t *spd, int cpuid);
int lend_cpus_sp(subprocess_descriptor_t *spd, int ncpus);
int lend_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Reclaim */
int reclaim_sp(subprocess_descriptor_t *spd);
int reclaim_cpu_sp(subprocess_descriptor_t *spd, int cpuid);
int reclaim_cpus_sp(subprocess_descriptor_t *spd, int ncpus);
int reclaim_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Acquire */
int acquire_sp(subprocess_descriptor_t *spd);
int acquire_cpu_sp(subprocess_descriptor_t *spd, int cpuid);
int acquire_cpus_sp(subprocess_descriptor_t *spd, int ncpus);
int acquire_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Return */
int return_all_sp(subprocess_descriptor_t *spd);
int return_cpu_sp(subprocess_descriptor_t *spd, int cpuid);
int return_cpu_mask_sp(subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* DROM Responsive */
int poll_drom_sp(subprocess_descriptor_t *spd, int *new_cpus, cpu_set_t *new_mask);

/* Misc */
int set_variable_sp(subprocess_descriptor_t *spd, const char *variable, const char *value);
int get_variable_sp(subprocess_descriptor_t *spd, const char *variable, char *value);
int print_variables_sp(subprocess_descriptor_t *spd);

#endif /* DLB_KERNEL_SP_H */
