/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "apis/dlb_talp.h"
#include "support/atomic.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct Options options_t;
typedef struct SubProcessDescriptor subprocess_descriptor_t;
typedef struct omptool_parallel_data_t omptool_parallel_data_t;

/* Monitoring regions */
struct dlb_monitor_t* monitoring_region_get_global_region(
        const subprocess_descriptor_t *spd);
const char* monitoring_region_get_global_region_name(void);
struct dlb_monitor_t* monitoring_region_register(
        const subprocess_descriptor_t *spd, const char* name);
int monitoring_region_reset(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_start(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_stop(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor);
bool monitoring_region_is_started(const struct dlb_monitor_t *monitor);
void monitoring_region_set_internal(struct dlb_monitor_t *monitor, bool internal);
int monitoring_region_report(const subprocess_descriptor_t *spd,
        const struct dlb_monitor_t *monitor);
int monitoring_regions_force_update(const subprocess_descriptor_t *spd);


/* TALP init / finalize */
void talp_init(subprocess_descriptor_t *spd);
void talp_finalize(subprocess_descriptor_t *spd);


/* TALP MPI functions */
void talp_mpi_init(const subprocess_descriptor_t *spd);
void talp_mpi_finalize(const subprocess_descriptor_t *spd);
void talp_into_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective);
void talp_out_of_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective);


/* TALP OpenMP functions */
void talp_openmp_init(pid_t, const options_t*);
void talp_openmp_finalize(void);
void talp_openmp_thread_begin();
void talp_openmp_thread_end(void);
void talp_openmp_parallel_begin(omptool_parallel_data_t *parallel_data);
void talp_openmp_parallel_end(omptool_parallel_data_t *parallel_data);
void talp_openmp_into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index);
void talp_openmp_into_parallel_implicit_barrier(
        omptool_parallel_data_t *parallel_data);
void talp_openmp_into_parallel_sync(omptool_parallel_data_t *parallel_data);
void talp_openmp_outof_parallel_sync(omptool_parallel_data_t *parallel_data);
void talp_openmp_task_create(void);
void talp_openmp_task_complete(void);
void talp_openmp_task_switch(void);


/* TALP collect functions for 3rd party programs */
int talp_query_pop_node_metrics(const char *name, struct dlb_node_metrics_t *node_metrics);


/* TALP collect functions for 1st party programs */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_pop_metrics_t *pop_metrics);
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_node_metrics_t *node_metrics);


#endif
