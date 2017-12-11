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

#include "LB_core/spd.h"
#include "apis/dlb_types.h"
#include "support/options.h"

#include <sched.h>
#include <stdbool.h>

/* Status */
int Initialize(subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask,
        const char *lb_args);
int Finish(subprocess_descriptor_t *spd);
int set_dlb_enabled(subprocess_descriptor_t *spd, bool enabled);
int set_max_parallelism(subprocess_descriptor_t *spd, int max);

/* MPI specific */
void IntoCommunication(void);
void OutOfCommunication(void);
void IntoBlockingCall(int is_iter, int is_single);
void OutOfBlockingCall(int is_iter);

/* Lend */
int lend(const subprocess_descriptor_t *spd);
int lend_cpu(const subprocess_descriptor_t *spd, int cpuid);
int lend_cpus(const subprocess_descriptor_t *spd, int ncpus);
int lend_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Reclaim */
int reclaim(const subprocess_descriptor_t *spd);
int reclaim_cpu(const subprocess_descriptor_t *spd, int cpuid);
int reclaim_cpus(const subprocess_descriptor_t *spd, int ncpus);
int reclaim_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Acquire */
int acquire_cpu(const subprocess_descriptor_t *spd, int cpuid);
int acquire_cpus(const subprocess_descriptor_t *spd, int ncpus);
int acquire_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Borrow */
int borrow(const subprocess_descriptor_t *spd);
int borrow_cpu(const subprocess_descriptor_t *spd, int cpuid);
int borrow_cpus(const subprocess_descriptor_t *spd, int ncpus);
int borrow_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* Return */
int return_all(const subprocess_descriptor_t *spd);
int return_cpu(const subprocess_descriptor_t *spd, int cpuid);
int return_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* DROM Responsive */
int poll_drom(const subprocess_descriptor_t *spd, int *new_cpus, cpu_set_t *new_mask);
int poll_drom_update(const subprocess_descriptor_t *spd);

/* Misc */
int check_cpu_availability(const subprocess_descriptor_t *spd, int cpuid);
int node_barrier(void);
int print_shmem(subprocess_descriptor_t *spd, int num_columns,
        dlb_printshmem_flags_t print_flags);

#endif //DLB_KERNEL_H
