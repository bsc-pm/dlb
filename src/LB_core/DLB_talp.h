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

#ifndef DLB_CORE_TALP_H
#define DLB_CORE_TALP_H

#include <sched.h>

struct SubProcessDescriptor;
struct dlb_monitor_t;
struct dlb_pop_metrics_t;
struct dlb_node_metrics_t;

/*  Initializes the module structures */
void talp_init(struct SubProcessDescriptor *spd);

/*  Finalizes the execution of the module and shows the final report if needed */
void talp_finalize(struct SubProcessDescriptor *spd);

/* Start MPI monitoring region */
void talp_mpi_init(const struct SubProcessDescriptor *spd);

/* Stop MPI monitoring region */
void talp_mpi_finalize(const struct SubProcessDescriptor *spd);

/*  Enables the cpuid/cpu_mask for the current process */
void talp_cpu_enable(const struct SubProcessDescriptor *spd, int cpuid);
void talp_cpuset_enable(const struct SubProcessDescriptor *spd, const cpu_set_t *cpu_mask);

/*  Disables the cpuid/cpu_mask for the current process */
void talp_cpu_disable(const struct SubProcessDescriptor *spd, int cpuid);
void talp_cpuset_disable(const struct SubProcessDescriptor *spd, const cpu_set_t *cpu_mask);

/* Set new cpu_mask */
void talp_cpuset_set(const struct SubProcessDescriptor *spd, const cpu_set_t *cpu_mask);

/*  Update the metrics when entering MPI */
void talp_in_mpi(const struct SubProcessDescriptor *spd);

/*  Update the metrics when going out MPI */
void talp_out_mpi(const struct SubProcessDescriptor *spd);

/* Obtain the implicit MPI region */
const struct dlb_monitor_t* monitoring_region_get_MPI_region(
        const struct SubProcessDescriptor *spd);

struct dlb_monitor_t* monitoring_region_register(const char* name);
int monitoring_region_reset(struct dlb_monitor_t *monitor);
int monitoring_region_start(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_stop(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_report(const struct SubProcessDescriptor *spd,
        const struct dlb_monitor_t *monitor);
int monitoring_regions_force_update(const struct SubProcessDescriptor *spd);

int talp_collect_pop_metrics(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor, struct dlb_pop_metrics_t *pop_metrics);

int talp_collect_node_metrics(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor, struct dlb_node_metrics_t *node_metrics);

#endif
