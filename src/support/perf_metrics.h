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

#include "LB_core/spd.h"
#include "support/options.h"

#include <stdint.h>


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

/* Compute POP metrics for the MPI model
 * (This funtion is actually not used anywhere) */
static inline void perf_metrics__compute_mpi_model(
        perf_metrics_mpi_t *metrics,
        int num_cpus,
        int num_nodes,
        int64_t elapsed_time,
        int64_t elapsed_useful,
        int64_t app_sum_useful,
        int64_t node_sum_useful) {

    if (elapsed_time > 0) {
        *metrics = (const perf_metrics_mpi_t) {
            .parallel_efficiency = (float)app_sum_useful / (elapsed_time * num_cpus),
            .communication_efficiency = (float)elapsed_useful / elapsed_time,
            .load_balance = (float)app_sum_useful / (elapsed_useful * num_cpus),
            .lb_in = (float)(node_sum_useful * num_nodes) / (elapsed_useful * num_cpus),
            .lb_out = (float)app_sum_useful / (node_sum_useful * num_nodes),
        };
    } else {
        *metrics = (const perf_metrics_mpi_t) {};
    }
}

/* Compute POP metrics for the MPI model, but with some inferred values:
 * (Only useful for node metrics) */
static inline void perf_metrics__infer_mpi_model(
        perf_metrics_mpi_t *metrics,
        int processes_per_node,
        int64_t node_sum_useful,
        int64_t node_sum_mpi,
        int64_t max_useful_time) {

    int64_t elapsed_time = (node_sum_useful + node_sum_mpi) / processes_per_node;
    if (elapsed_time > 0) {
        *metrics = (const perf_metrics_mpi_t) {
            .parallel_efficiency = (float)node_sum_useful / (node_sum_useful + node_sum_mpi),
            .communication_efficiency = (float)max_useful_time / elapsed_time,
            .load_balance = ((float)node_sum_useful / processes_per_node) / max_useful_time,
        };
    } else {
        *metrics = (const perf_metrics_mpi_t) {};
    }
}


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

/* Computed efficiency metrics for the POP hybrid model */
typedef struct perf_metrics_hybrid_t {
    float parallel_efficiency;
    float mpi_parallel_efficiency;
    float mpi_communication_efficiency;
    float mpi_load_balance;
    float mpi_load_balance_in;
    float mpi_load_balance_out;
    float omp_parallel_efficiency;
    float omp_load_balance;
    float omp_scheduling_efficiency;
    float omp_serialization_efficiency;
} perf_metrics_hybrid_t;

/* Compute POP metrics for the hybrid MPI + OpenMP model
 * (Ver. 1: All metrics are multiplicative, but some of them are > 1) */
static inline void perf_metrics__compute_hybrid_model_v1(
        perf_metrics_hybrid_t *metrics,
        const pop_base_metrics_t *base_metrics) {

    int64_t useful_time             = base_metrics->useful_time;
    int64_t mpi_time                = base_metrics->mpi_time;
    int64_t omp_load_imbalance_time = base_metrics->omp_load_imbalance_time;
    int64_t omp_scheduling_time     = base_metrics->omp_scheduling_time;
    int64_t omp_serialization_time  = base_metrics->omp_serialization_time;
    double  useful_normd_app        = base_metrics->useful_normd_app;
    double  mpi_normd_app           = base_metrics->mpi_normd_app;
    double  max_useful_normd_proc   = base_metrics->max_useful_normd_proc;
    double  max_useful_normd_node   = base_metrics->max_useful_normd_node;

    /* Active is the union of all times (CPU not disabled) */
    int64_t sum_active = useful_time + mpi_time + omp_load_imbalance_time +
        omp_scheduling_time + omp_serialization_time;

    /* Equivalent to all CPU time if OMP was not present */
    int64_t sum_active_not_omp = useful_time + mpi_time;

    /* Compute output metrics */
    *metrics = (const perf_metrics_hybrid_t) {
        .parallel_efficiency = (float)useful_time / sum_active,
        .mpi_parallel_efficiency = (float)useful_time / sum_active_not_omp,
        .mpi_communication_efficiency =
            max_useful_normd_proc / (useful_normd_app + mpi_normd_app),
        .mpi_load_balance = useful_normd_app / max_useful_normd_proc,
        .mpi_load_balance_in = max_useful_normd_node / max_useful_normd_proc,
        .mpi_load_balance_out = useful_normd_app / max_useful_normd_node,
        .omp_parallel_efficiency = (float)sum_active_not_omp / sum_active,
        .omp_load_balance = (float)(sum_active_not_omp + omp_serialization_time)
            / (sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time),
        .omp_scheduling_efficiency =
            (float)(sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time)
            / (sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time
                    + omp_scheduling_time),
        .omp_serialization_efficiency = (float)sum_active_not_omp
            / (sum_active_not_omp + omp_serialization_time),
    };
}

/* Compute POP metrics for the hybrid MPI + OpenMP model (Ver. 2: PE != MPE * OPE) */
static inline void perf_metrics__compute_hybrid_model_v2(
        perf_metrics_hybrid_t *metrics,
        const pop_base_metrics_t *base_metrics) {

    int64_t useful_time             = base_metrics->useful_time;
    int64_t mpi_time                = base_metrics->mpi_time;
    int64_t omp_load_imbalance_time = base_metrics->omp_load_imbalance_time;
    int64_t omp_scheduling_time     = base_metrics->omp_scheduling_time;
    int64_t omp_serialization_time  = base_metrics->omp_serialization_time;
    double  useful_normd_app        = base_metrics->useful_normd_app;
    double  max_useful_normd_proc   = base_metrics->max_useful_normd_proc;
    double  max_useful_normd_node   = base_metrics->max_useful_normd_node;
    double  mpi_normd_of_max_useful = base_metrics->mpi_normd_of_max_useful;

    /* Active is the union of all times (CPU not disabled) */
    int64_t sum_active = useful_time + mpi_time + omp_load_imbalance_time +
        omp_scheduling_time + omp_serialization_time;

    /* Equivalent to all CPU time if OMP was not present */
    int64_t sum_active_not_omp = useful_time + mpi_time;

    /* Compute output metrics */
    *metrics = (const perf_metrics_hybrid_t) {
        .parallel_efficiency = (float)useful_time / sum_active,
        .mpi_parallel_efficiency =
            useful_normd_app / (max_useful_normd_proc + mpi_normd_of_max_useful),
        .mpi_communication_efficiency =
            max_useful_normd_proc / (max_useful_normd_proc + mpi_normd_of_max_useful),
        .mpi_load_balance = useful_normd_app / max_useful_normd_proc,
        .mpi_load_balance_in = max_useful_normd_node / max_useful_normd_proc,
        .mpi_load_balance_out = useful_normd_app / max_useful_normd_node,
        .omp_parallel_efficiency = (float)sum_active_not_omp / sum_active,
        .omp_load_balance = (float)(sum_active_not_omp + omp_serialization_time)
            / (sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time),
        .omp_scheduling_efficiency =
            (float)(sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time)
            / (sum_active_not_omp + omp_serialization_time + omp_load_imbalance_time
                    + omp_scheduling_time),
        .omp_serialization_efficiency = (float)sum_active_not_omp
            / (sum_active_not_omp + omp_serialization_time),
    };
}

/* Actual 'public' function. DLB_talp.c invokes this function with base_metrics
 * as input, and the computed pop_metrics as output. */
static inline void talp_base_metrics_to_pop_metrics(const char *monitor_name,
        const pop_base_metrics_t *base_metrics, dlb_pop_metrics_t *pop_metrics) {

    /* Compute POP metrics */
    perf_metrics_hybrid_t metrics = {0};

    if (base_metrics->useful_time > 0) {

        switch(thread_spd->options.talp_model) {
            case TALP_MODEL_HYBRID_V1:
                perf_metrics__compute_hybrid_model_v1(&metrics, base_metrics);
                break;
            case TALP_MODEL_HYBRID_V2:
                perf_metrics__compute_hybrid_model_v2(&metrics, base_metrics);
                break;
        };
    }

    /* Initialize structure */
    *pop_metrics = (const dlb_pop_metrics_t) {
        .num_cpus                     = base_metrics->num_cpus,
        .num_mpi_ranks                = base_metrics->num_mpi_ranks,
        .num_nodes                    = base_metrics->num_nodes,
        .avg_cpus                     = base_metrics->avg_cpus,
        .cycles                       = base_metrics->cycles,
        .instructions                 = base_metrics->instructions,
        .num_measurements             = base_metrics->num_measurements,
        .num_mpi_calls                = base_metrics->num_mpi_calls,
        .num_omp_parallels            = base_metrics->num_omp_parallels,
        .num_omp_tasks                = base_metrics->num_omp_tasks,
        .elapsed_time                 = base_metrics->elapsed_time,
        .useful_time                  = base_metrics->useful_time,
        .mpi_time                     = base_metrics->mpi_time,
        .omp_load_imbalance_time      = base_metrics->omp_load_imbalance_time,
        .omp_scheduling_time          = base_metrics->omp_scheduling_time,
        .omp_serialization_time       = base_metrics->omp_serialization_time,
        .useful_normd_app             = base_metrics->useful_normd_app,
        .mpi_normd_app                = base_metrics->mpi_normd_app,
        .max_useful_normd_proc        = base_metrics->max_useful_normd_proc,
        .max_useful_normd_node        = base_metrics->max_useful_normd_node,
        .mpi_normd_of_max_useful      = base_metrics->mpi_normd_of_max_useful,
        .parallel_efficiency          = metrics.parallel_efficiency,
        .mpi_parallel_efficiency      = metrics.mpi_parallel_efficiency,
        .mpi_communication_efficiency = metrics.mpi_communication_efficiency,
        .mpi_load_balance             = metrics.mpi_load_balance,
        .mpi_load_balance_in          = metrics.mpi_load_balance_in,
        .mpi_load_balance_out         = metrics.mpi_load_balance_out,
        .omp_parallel_efficiency      = metrics.omp_parallel_efficiency,
        .omp_load_balance             = metrics.omp_load_balance,
        .omp_scheduling_efficiency    = metrics.omp_scheduling_efficiency,
        .omp_serialization_efficiency = metrics.omp_serialization_efficiency,
    };
    snprintf(pop_metrics->name, DLB_MONITOR_NAME_MAX, "%s", monitor_name);
}

#endif /* PERF_METRICS_H */
