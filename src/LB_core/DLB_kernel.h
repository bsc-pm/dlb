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

#ifndef DLB_KERNEL_H
#define DLB_KERNEL_H

#include "apis/dlb_types.h"
#include "support/options.h"

#include <sched.h>
#include <stdbool.h>

/* Status */
int Initialize(const cpu_set_t *mask, const char *lb_args);
int Finish(void);
int set_dlb_enabled(bool enabled);
int set_max_parallelism(int max);

/* Callbacks */
int callback_set(dlb_callbacks_t which, dlb_callback_t callback);
int callback_get(dlb_callbacks_t which, dlb_callback_t callback);

/* MPI specific */
void IntoCommunication(void);
void OutOfCommunication(void);
void IntoBlockingCall(int is_iter, int is_single);
void OutOfBlockingCall(int is_iter);

/* Lend */
int lend(void);
int lend_cpu(int cpuid);
int lend_cpus(int ncpus);
int lend_cpu_mask(const cpu_set_t *mask);

/* Reclaim */
int reclaim(void);
int reclaim_cpu(int cpuid);
int reclaim_cpus(int ncpus);
int reclaim_cpu_mask(const cpu_set_t *mask);

/* Acquire */
int acquire(void);
int acquire_cpu(int cpuid);
int acquire_cpus(int ncpus);
int acquire_cpu_mask(const cpu_set_t* mask);

/* Return */
int return_all(void);
int return_cpu(int cpuid);

/* DROM Responsive */
int poll_drom(int *new_threads, cpu_set_t *new_mask);

/* Misc */
int check_cpu_availability(int cpuid);
int barrier(void);
int set_variable(const char *variable, const char *value);
int get_variable(const char *variable, char *value);
int print_variables(void);
int print_shmem(void);


// Others
const options_t* get_global_options(void);

#endif //DLB_KERNEL_H
