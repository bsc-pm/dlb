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

#ifndef DLB_KERNEL_H
#define DLB_KERNEL_H

#include "LB_core/spd.h"
#include "apis/dlb_types.h"
#include "support/options.h"

#include <sched.h>
#include <stdbool.h>

typedef struct dlb_mpi_flags_t {
    bool is_blocking;
    bool is_collective;
    bool lewi_mpi;
} dlb_mpi_flags_t;

/* Status */
int Initialize(subprocess_descriptor_t *spd, pid_t id, int ncpus,
        const cpu_set_t *mask, const char *lb_args);
int Finish(subprocess_descriptor_t *spd);
int PreInitialize(subprocess_descriptor_t *spd, const cpu_set_t *mask,
        const char *lb_args);
int set_lewi_enabled(subprocess_descriptor_t *spd, bool enabled);
int set_max_parallelism(subprocess_descriptor_t *spd, int max);
int unset_max_parallelism(subprocess_descriptor_t *spd);

/* MPI specific */
void into_mpi(dlb_mpi_flags_t flags);
void out_of_mpi(dlb_mpi_flags_t flags);

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
int acquire_cpus_in_mask(const subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask);

/* Borrow */
int borrow(const subprocess_descriptor_t *spd);
int borrow_cpu(const subprocess_descriptor_t *spd, int cpuid);
int borrow_cpus(const subprocess_descriptor_t *spd, int ncpus);
int borrow_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
int borrow_cpus_in_mask(const subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask);

/* Return */
int return_all(const subprocess_descriptor_t *spd);
int return_cpu(const subprocess_descriptor_t *spd, int cpuid);
int return_cpu_mask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

/* DROM Responsive */
int poll_drom(const subprocess_descriptor_t *spd, int *new_cpus, cpu_set_t *new_mask);
int poll_drom_update(const subprocess_descriptor_t *spd);
int drom_setprocessmask(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags);

/* Misc */
int check_cpu_availability(const subprocess_descriptor_t *spd, int cpuid);
int print_shmem(subprocess_descriptor_t *spd, int num_columns,
        dlb_printshmem_flags_t print_flags);
int set_observer_role(bool is_observer);

#endif //DLB_KERNEL_H
