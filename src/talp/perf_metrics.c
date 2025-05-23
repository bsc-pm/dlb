/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#include "talp/perf_metrics.h"

#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "support/debug.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <stddef.h>
#include <stdio.h>

/*********************************************************************************/
/*    POP metrics - pure MPI model                                               */
/*********************************************************************************/

/* Compute POP metrics for the MPI model
 * (This funtion is actually not used anywhere) */
static inline void perf_metrics__compute_mpi_model(
        perf_metrics_mpi_t *metrics,
        int num_cpus,
        int num_nodes,
        int64_t elapsed_time,
        int64_t elapsed_useful,
        int64_t app_sum_useful,
        int64_t node_sum_useful)  __attribute__((unused));
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
void perf_metrics__infer_mpi_model(
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

#ifdef MPI_LIB

/* The following node and app reductions are needed to compute POP metrics: */

/*** Node reduction ***/

/* Data type to reduce among processes in node */
typedef struct node_reduction_t {
    bool node_used;
    int cpus_node;
    int64_t sum_useful;
} node_reduction_t;

/* Function called in the MPI node reduction */
static void mpi_node_reduction_fn(void *invec, void *inoutvec, int *len,
        MPI_Datatype *datatype) {
    node_reduction_t *in = invec;
    node_reduction_t *inout = inoutvec;

    int _len = *len;
    for (int i = 0; i < _len; ++i) {
        if (in[i].node_used) {
            inout[i].node_used = true;
            inout[i].cpus_node += in[i].cpus_node;
            inout[i].sum_useful += in[i].sum_useful;
        }
    }
}

/* Function to perform the reduction at node level */
static void reduce_pop_metrics_node_reduction(node_reduction_t *node_reduction,
        const dlb_monitor_t *monitor) {

    const node_reduction_t node_reduction_send = {
        .node_used = monitor->num_measurements > 0,
        .cpus_node = monitor->num_cpus,
        .sum_useful = monitor->useful_time,
    };

    /* MPI type: int64_t */
    MPI_Datatype mpi_int64_type = get_mpi_int64_type();

    /* MPI struct type: node_reduction_t */
    MPI_Datatype mpi_node_reduction_type;
    {
        int count = 3;
        int blocklengths[] = {1, 1, 1};
        MPI_Aint displacements[] = {
            offsetof(node_reduction_t, node_used),
            offsetof(node_reduction_t, cpus_node),
            offsetof(node_reduction_t, sum_useful)};
        MPI_Datatype types[] = {MPI_C_BOOL, MPI_INT, mpi_int64_type};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(node_reduction_t),
                &mpi_node_reduction_type);
        PMPI_Type_commit(&mpi_node_reduction_type);
    }

    /* Define MPI operation */
    MPI_Op node_reduction_op;
    PMPI_Op_create(mpi_node_reduction_fn, true, &node_reduction_op);

    /* MPI reduction */
    PMPI_Reduce(&node_reduction_send, node_reduction, 1,
            mpi_node_reduction_type, node_reduction_op,
            0, getNodeComm());

    /* Free MPI types */
    PMPI_Type_free(&mpi_node_reduction_type);
    PMPI_Op_free(&node_reduction_op);
}

/** App reduction ***/

/* Data type to reduce among processes in application */
typedef struct app_reduction_t {
    int num_cpus;
    int num_nodes;
    float avg_cpus;
    double cycles;
    double instructions;
    int64_t num_measurements;
    int64_t num_mpi_calls;
    int64_t num_omp_parallels;
    int64_t num_omp_tasks;
    int64_t elapsed_time;
    int64_t useful_time;
    int64_t mpi_time;
    int64_t omp_load_imbalance_time;
    int64_t omp_scheduling_time;
    int64_t omp_serialization_time;
    double  max_useful_normd_proc;
    double  max_useful_normd_node;
    double  mpi_normd_of_max_useful;
} app_reduction_t;

/* Function called in the MPI app reduction */
static void mpi_reduction_fn(void *invec, void *inoutvec, int *len,
        MPI_Datatype *datatype) {
    app_reduction_t *in = invec;
    app_reduction_t *inout = inoutvec;

    int _len = *len;
    for (int i = 0; i < _len; ++i) {
        inout[i].num_cpus                += in[i].num_cpus;
        inout[i].num_nodes               += in[i].num_nodes;
        inout[i].avg_cpus                += in[i].avg_cpus;
        inout[i].cycles                  += in[i].cycles;
        inout[i].instructions            += in[i].instructions;
        inout[i].num_measurements        += in[i].num_measurements;
        inout[i].num_mpi_calls           += in[i].num_mpi_calls;
        inout[i].num_omp_parallels       += in[i].num_omp_parallels;
        inout[i].num_omp_tasks           += in[i].num_omp_tasks;
        inout[i].elapsed_time             = max_int64(inout[i].elapsed_time, in[i].elapsed_time);
        inout[i].useful_time             += in[i].useful_time;
        inout[i].mpi_time                += in[i].mpi_time;
        inout[i].omp_load_imbalance_time += in[i].omp_load_imbalance_time;
        inout[i].omp_scheduling_time     += in[i].omp_scheduling_time;
        inout[i].omp_serialization_time  += in[i].omp_serialization_time;
        inout[i].max_useful_normd_node  =
            max_int64(inout[i].max_useful_normd_node, in[i].max_useful_normd_node);
        if (in[i].max_useful_normd_proc > inout[i].max_useful_normd_proc) {
            inout[i].max_useful_normd_proc = in[i].max_useful_normd_proc;
            inout[i].mpi_normd_of_max_useful = in[i].mpi_normd_of_max_useful;
        }
    }
}

/* Function to perform the reduction at application level */
static void reduce_pop_metrics_app_reduction(app_reduction_t *app_reduction,
        const node_reduction_t *node_reduction, const dlb_monitor_t *monitor,
        bool all_to_all) {

    double max_useful_normd_proc = monitor->num_cpus == 0 ? 0.0
        : (double)monitor->useful_time / monitor->num_cpus;
    double max_useful_normd_node = _process_id != 0 ? 0.0
        : node_reduction->cpus_node == 0 ? 0.0
        : (double)node_reduction->sum_useful / node_reduction->cpus_node;
    double mpi_normd_of_max_useful = monitor->num_cpus == 0 ? 0.0
        : (double)monitor->mpi_time / monitor->num_cpus;

    const app_reduction_t app_reduction_send = {
        .num_cpus                = monitor->num_cpus,
        .num_nodes               = _process_id == 0 && node_reduction->node_used ? 1 : 0,
        .avg_cpus                = monitor->avg_cpus,
        .cycles                  = (double)monitor->cycles,
        .instructions            = (double)monitor->instructions,
        .num_measurements        = monitor->num_measurements,
        .num_mpi_calls           = monitor->num_mpi_calls,
        .num_omp_parallels       = monitor->num_omp_parallels,
        .num_omp_tasks           = monitor->num_omp_tasks,
        .elapsed_time            = monitor->elapsed_time,
        .useful_time             = monitor->useful_time,
        .mpi_time                = monitor->mpi_time,
        .omp_load_imbalance_time = monitor->omp_load_imbalance_time,
        .omp_scheduling_time     = monitor->omp_scheduling_time,
        .omp_serialization_time  = monitor->omp_serialization_time,
        .max_useful_normd_proc   = max_useful_normd_proc,
        .max_useful_normd_node   = max_useful_normd_node,
        .mpi_normd_of_max_useful = mpi_normd_of_max_useful,
    };

    /* MPI type: int64_t */
    MPI_Datatype mpi_int64_type = get_mpi_int64_type();

    /* MPI struct type: app_reduction_t */
    MPI_Datatype mpi_app_reduction_type;
    {
        enum {count = 18};
        int blocklengths[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        MPI_Aint displacements[] = {
            offsetof(app_reduction_t, num_cpus),
            offsetof(app_reduction_t, num_nodes),
            offsetof(app_reduction_t, avg_cpus),
            offsetof(app_reduction_t, cycles),
            offsetof(app_reduction_t, instructions),
            offsetof(app_reduction_t, num_measurements),
            offsetof(app_reduction_t, num_mpi_calls),
            offsetof(app_reduction_t, num_omp_parallels),
            offsetof(app_reduction_t, num_omp_tasks),
            offsetof(app_reduction_t, elapsed_time),
            offsetof(app_reduction_t, useful_time),
            offsetof(app_reduction_t, mpi_time),
            offsetof(app_reduction_t, omp_load_imbalance_time),
            offsetof(app_reduction_t, omp_scheduling_time),
            offsetof(app_reduction_t, omp_serialization_time),
            offsetof(app_reduction_t, max_useful_normd_proc),
            offsetof(app_reduction_t, max_useful_normd_node),
            offsetof(app_reduction_t, mpi_normd_of_max_useful)};
        MPI_Datatype types[] = {MPI_INT, MPI_INT, MPI_FLOAT, MPI_DOUBLE,
            MPI_DOUBLE, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type, mpi_int64_type, MPI_DOUBLE,
            MPI_DOUBLE, MPI_DOUBLE};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(app_reduction_t),
                &mpi_app_reduction_type);
        PMPI_Type_commit(&mpi_app_reduction_type);

        static_ensure(sizeof(blocklengths)/sizeof(blocklengths[0]) == count,
                "blocklengths size mismatch");
        static_ensure(sizeof(displacements)/sizeof(displacements[0]) == count,
                "displacements size mismatch");
        static_ensure(sizeof(types)/sizeof(types[0]) == count,
                "types size mismatch");
    }

    /* Define MPI operation */
    MPI_Op app_reduction_op;
    MPI_Op_create(mpi_reduction_fn, true, &app_reduction_op);

    /* MPI reduction */
    if (!all_to_all) {
        PMPI_Reduce(&app_reduction_send, app_reduction, 1,
                mpi_app_reduction_type, app_reduction_op,
                0, getWorldComm());
    } else {
        PMPI_Allreduce(&app_reduction_send, app_reduction, 1,
                mpi_app_reduction_type, app_reduction_op,
                getWorldComm());
    }

    /* Free MPI types */
    PMPI_Type_free(&mpi_app_reduction_type);
    PMPI_Op_free(&app_reduction_op);
}

#endif



#if MPI_LIB
/* Construct a base metrics struct out of a monitor  */
void perf_metrics__reduce_monitor_into_base_metrics(pop_base_metrics_t *base_metrics,
        const dlb_monitor_t *monitor, bool all_to_all) {

    /* First, reduce some values among processes in the node,
     * needed to compute pop metrics */
    node_reduction_t node_reduction = {0};
    reduce_pop_metrics_node_reduction(&node_reduction, monitor);

    /* With the node reduction, reduce again among all process */
    app_reduction_t app_reduction = {0};
    reduce_pop_metrics_app_reduction(&app_reduction, &node_reduction,
            monitor, all_to_all);

    /* Finally, fill output base_metrics... */

    int num_mpi_ranks;
    PMPI_Comm_size(getWorldComm(), &num_mpi_ranks);

    /* These values do not need a specific MPI reduction and can be deduced
     * from the already reduced number of CPUs */
    double useful_normd_app = app_reduction.num_cpus == 0 ? 0.0
        : (double)app_reduction.useful_time / app_reduction.num_cpus;
    double mpi_normd_app = app_reduction.num_cpus == 0 ? 0.0
        : (double)app_reduction.mpi_time / app_reduction.num_cpus;

    *base_metrics = (const pop_base_metrics_t) {
        .num_cpus                = app_reduction.num_cpus,
        .num_mpi_ranks           = num_mpi_ranks,
        .num_nodes               = app_reduction.num_nodes,
        .avg_cpus                = app_reduction.avg_cpus,
        .cycles                  = app_reduction.cycles,
        .instructions            = app_reduction.instructions,
        .num_measurements        = app_reduction.num_measurements,
        .num_mpi_calls           = app_reduction.num_mpi_calls,
        .num_omp_parallels       = app_reduction.num_omp_parallels,
        .num_omp_tasks           = app_reduction.num_omp_tasks,
        .elapsed_time            = app_reduction.elapsed_time,
        .useful_time             = app_reduction.useful_time,
        .mpi_time                = app_reduction.mpi_time,
        .omp_load_imbalance_time = app_reduction.omp_load_imbalance_time,
        .omp_scheduling_time     = app_reduction.omp_scheduling_time,
        .omp_serialization_time  = app_reduction.omp_serialization_time,
        .useful_normd_app        = useful_normd_app,
        .mpi_normd_app           = mpi_normd_app,
        .max_useful_normd_proc   = app_reduction.max_useful_normd_proc,
        .max_useful_normd_node   = app_reduction.max_useful_normd_node,
        .mpi_normd_of_max_useful = app_reduction.mpi_normd_of_max_useful,
    };
}
#endif

/* Compute POP metrics out of a base metrics struct */
void perf_metrics__base_to_pop_metrics(const char *monitor_name,
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
