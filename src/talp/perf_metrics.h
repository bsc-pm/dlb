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

#ifndef PERF_METRICS_H
#define PERF_METRICS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct dlb_monitor_t dlb_monitor_t;
typedef struct dlb_pop_metrics_t dlb_pop_metrics_t;

/*********************************************************************************/
/*    POP metrics - pure MPI model                                               */
/*********************************************************************************/

/* POP metrics for pure MPI executions */
typedef struct perf_metrics_mpi_t {
    float parallel_efficiency;
    float communication_efficiency;
    float load_balance;
    float lb_in;
    float lb_out;
} perf_metrics_mpi_t;

void perf_metrics__infer_mpi_model(
        perf_metrics_mpi_t *metrics,
        int processes_per_node,
        int64_t node_sum_useful,
        int64_t node_sum_mpi,
        int64_t max_useful_time);


/*********************************************************************************/
/*    POP metrics - hybrid MPI + OpenMP model                                    */
/*********************************************************************************/

/* Internal struct to contain everything that's needed to actually construct a
 * dlb_pop_metrics_t. Can be thought of as an abstract to the app_reduction_t
 * combining serial and parallel program flow. Everything in here is coming
 * directly from measurement, or computed in an MPI reduction. */
typedef struct pop_base_metrics_t {
    /* Resources */
    int     num_cpus;
    int     num_mpi_ranks;
    int     num_nodes;
    float   avg_cpus;
    /* Hardware counters */
    double  cycles;
    double  instructions;
    /* Statistics */
    int64_t num_measurements;
    int64_t num_mpi_calls;
    int64_t num_omp_parallels;
    int64_t num_omp_tasks;
    /* Sum of times among all processes */
    int64_t elapsed_time;
    int64_t useful_time;
    int64_t mpi_time;
    int64_t omp_load_imbalance_time;
    int64_t omp_scheduling_time;
    int64_t omp_serialization_time;
    /* Normalized times by the number of assigned CPUs */
    double  useful_normd_app;        /* Useful time normalized by num. CPUs at application level */
    double  mpi_normd_app;           /* MPI time normalized by num. CPUs at application level */
    double  max_useful_normd_proc;   /* Max value of useful times normalized at process level */
    double  max_useful_normd_node;   /* Max value of useful times normalized at node level */
    double  mpi_normd_of_max_useful; /* MPI time normalized at process level of the process with
                                        the max useful time */
} pop_base_metrics_t;


#if MPI_LIB
void perf_metrics__reduce_monitor_into_base_metrics(pop_base_metrics_t *base_metrics,
        const dlb_monitor_t *monitor, bool all_to_all);
#endif

void perf_metrics__base_to_pop_metrics(const char *monitor_name,
        const pop_base_metrics_t *base_metrics, dlb_pop_metrics_t *pop_metrics);

#endif /* PERF_METRICS_H */
