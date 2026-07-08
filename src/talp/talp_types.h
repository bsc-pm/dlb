/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#ifndef TALP_TYPES_H
#define TALP_TYPES_H

#include "apis/dlb_talp.h"
#include "apis/dlb_types.h"
#include "support/atomic.h"
#include "support/gtree.h"
#include "support/gslist.h"

#include <pthread.h>

/*********************************************************************************/
/*    TALP sample and macrosample                                                */
/*********************************************************************************/

typedef enum {
    TALP_STATE_DISABLED,
    TALP_STATE_USEFUL,
    TALP_STATE_NOT_USEFUL_MPI,
    TALP_STATE_NOT_USEFUL_OMP_IN,
    TALP_STATE_NOT_USEFUL_OMP_OUT,
    TALP_STATE_NOT_USEFUL_GPU,
} talp_sample_state_t;

typedef struct {
    int64_t useful;
    int64_t not_useful_mpi;
    int64_t not_useful_omp_during_mpi;
    int64_t not_useful_omp_in;
    int64_t not_useful_omp_in_lb;
    int64_t not_useful_omp_in_sched;
    int64_t not_useful_omp_out;
    int64_t not_useful_gpu;
} sample_timers_t;

/* same as sample_timers_t, but no not_useful_omp_in */
typedef struct {
    int64_t useful;
    int64_t not_useful_mpi;
    int64_t not_useful_omp_during_mpi;
    int64_t not_useful_omp_in_lb;
    int64_t not_useful_omp_in_sched;
    int64_t not_useful_omp_out;
    int64_t not_useful_gpu;
} macro_timers_t;

typedef struct {
    int64_t useful;
    int64_t communication;
    int64_t inactive;
} gpu_timers_t;

typedef struct {
    int64_t cycles;
    int64_t instructions;
} hw_counters_t;

typedef struct {
    int64_t num_mpi_calls;
    int64_t num_omp_parallels;
    int64_t num_omp_tasks;
    int64_t num_gpu_runtime_calls;
} event_stats_t;


/* The sample contains the temporary per-thread accumulated values of all the
 * measured metrics. Main thread, during sequential code by contract, aggregates
 * samples from all threads into macrosamples.
 * The sample starts and ends on each one of the following scenarios:
 *  - MPI Init/Finalize
 *  - a region starts or stops
 *  - a request from the API
 *
 *  On OpenMP parallel regions, _lb and _sched need to be computed for that region
 *  so the encountering thread will have the representative time of all participating
 *  threads.
 */
typedef struct DLB_ALIGN_CACHE talp_sample_t {
    // Hot fields: thread-owned, frequently written
    sample_timers_t     timers;
    hw_counters_t       counters;
    event_stats_t       stats;
    talp_sample_state_t state;
    int64_t             last_updated_ts;    // timestamp of the last sample update
    int64_t             generation_ts;      // timestamp of the start of the generation since the
                                            // last update
    cpu_set_t           cpu_mask;           // CPUs this sample has been observed on

    // Cold fields: atomic variables read/written at parallel-end
    struct DLB_ALIGN_CACHE {
        atomic_int_least64_t last_parallel_end_ts;          // primary cross-thread writes
    };
} talp_sample_t;

/* The macrosample is a temporary aggregation of all metrics in samples of all,
 * or a subset of, threads. It is only constructed when samples are aggregated.
 * The macrosample is then used to update all started monitoring regions. */
typedef struct talp_macrosample_t {
    macro_timers_t timers;
    gpu_timers_t   gpu_timers;
    hw_counters_t  counters;
    event_stats_t  stats;
    cpu_set_t      cpu_mask;
    int            num_samples;
} talp_macrosample_t;


/*********************************************************************************/
/*    General TALP data                                                          */
/*********************************************************************************/

/* TALP flags for talp_info */
typedef struct talp_flags_t {
    bool have_shmem:1;          /* whether to record data in shmem */
    bool have_minimal_shmem:1;  /* whether to create a shmem for the global region */
    bool external_profiler:1;   /* whether to update shmem on every sample */
    bool have_mpi:1;            /* whether TALP regions have MPI events */
    bool have_openmp:1;         /* whether TALP regions have OpenMP events */
    bool have_gpu:1;            /* whether TALP regions have GPU events */
    bool have_hwc:1;            /* whether TALP regions have HWC events */
} talp_flags_t;

/* Collection of samples and metadada needed for aggregation */
typedef struct sample_registry_t {
    talp_sample_t         **samples;             /* Array of per-thread samples */
    int                   num_samples;           /* Number of samples */
    pthread_mutex_t       mutex;                 /* Protects samples */
    atomic_int_least64_t  current_generation_ts; /* Global aggregation generation timestamp */
} sample_registry_t;

/* TALP info per spd */
typedef struct talp_info_t {
    talp_flags_t      flags;
    int               num_cpus;        /* Number of CPUs in the initial process mask */
    dlb_monitor_t     *monitor;        /* Convenience pointer to the global region */
    GTree             *regions;        /* Tree of monitoring regions */
    GSList            *open_regions;   /* List of open regions */
    pthread_mutex_t   regions_mutex;   /* Mutex to protect regions allocation/iteration */
    sample_registry_t sample_registry; /* List of per-thread samples */
} talp_info_t;

/* Private data per monitor */
typedef struct monitor_data_t {
    int id;
    int node_shared_id;                 /* id for allocating region in the shmem */
    struct {
        bool started:1;
        bool internal:1;                /* internal regions are not reported */
        bool enabled:1;
    } flags;
    cpu_set_t cpu_mask;                 /* CPUs this region has been observed on */
} monitor_data_t;


/*********************************************************************************/
/*    POP base metrics                                                           */
/*********************************************************************************/

/* Internal struct to contain everything that's needed to actually construct a
 * dlb_pop_metrics_t. Everything in here is coming directly from measurement,
 * or computed in an MPI reduction. */
#define FOR_POP_BASE_METRICS_FIELDS(DO) \
    /* Resources */                                             \
    DO(num_cpus,                    int,        MPI_INT)        \
    DO(num_available_cpus,          int,        MPI_INT)        \
    DO(num_omp_threads,             int,        MPI_INT)        \
    DO(num_mpi_ranks,               int,        MPI_INT)        \
    DO(num_nodes,                   int,        MPI_INT)        \
    DO(avg_cpus,                    float,      MPI_FLOAT)      \
    DO(num_gpus,                    int,        MPI_INT)        \
    /* Hardware counters */                                     \
    DO(cycles,                      double,     MPI_DOUBLE)     \
    DO(instructions,                double,     MPI_DOUBLE)     \
    /* Statistics */                                            \
    DO(num_measurements,            int64_t,    mpi_int64_type) \
    DO(num_mpi_calls,               int64_t,    mpi_int64_type) \
    DO(num_omp_parallels,           int64_t,    mpi_int64_type) \
    DO(num_omp_tasks,               int64_t,    mpi_int64_type) \
    DO(num_gpu_runtime_calls,       int64_t,    mpi_int64_type) \
    /* Sum of Host times among all processes */                 \
    DO(elapsed_time,                int64_t,    mpi_int64_type) \
    DO(useful_time,                 int64_t,    mpi_int64_type) \
    DO(mpi_time,                    int64_t,    mpi_int64_type) \
    DO(mpi_worker_idle_time,        int64_t,    mpi_int64_type) \
    DO(omp_load_imbalance_time,     int64_t,    mpi_int64_type) \
    DO(omp_scheduling_time,         int64_t,    mpi_int64_type) \
    DO(omp_serialization_time,      int64_t,    mpi_int64_type) \
    DO(gpu_runtime_time,            int64_t,    mpi_int64_type) \
    /* Normalized Host times by the number of assigned CPUs */  \
    DO(min_mpi_normd_proc,          double,     MPI_DOUBLE)     \
    DO(min_mpi_normd_node,          double,     MPI_DOUBLE)     \
    /* Sum of Device times among all processes */               \
    DO(gpu_useful_time,             int64_t,    mpi_int64_type) \
    DO(gpu_communication_time,      int64_t,    mpi_int64_type) \
    DO(gpu_inactive_time,           int64_t,    mpi_int64_type) \
    /* Device Max Times */                                      \
    DO(max_gpu_useful_time,         int64_t,    mpi_int64_type) \
    DO(max_gpu_active_time,         int64_t,    mpi_int64_type)

typedef struct pop_base_metrics_t {
    #define DEFINE_C_TYPES(name, c_type, mpi_type) c_type name;
    FOR_POP_BASE_METRICS_FIELDS(DEFINE_C_TYPES)
    #undef DEFINE_C_TYPES
} pop_base_metrics_t;


/*********************************************************************************/
/*    Public TALP structs                                                        */
/*********************************************************************************/

/* The following lists correspond to public structs, and although those need to
 * be defined in the headers, we'll use the macro internally when possible.
 * They're also used for testing purposes to check that both definitions match.
 * REMINDER: Always keep in sync with the public definitions.
 */

/* dlb_monitor_t: per-process region metrics */
#define FOR_DLB_MONITOR_FIELDS(DO)                                  \
    DO(name,                        const char*,    address_type)   \
    DO(num_cpus,                    int,            MPI_INT)        \
    DO(num_omp_threads,             int,            MPI_INT)        \
    DO(avg_cpus,                    float,          MPI_FLOAT)      \
    DO(cycles,                      int64_t,        mpi_int64_type) \
    DO(instructions,                int64_t,        mpi_int64_type) \
    DO(num_measurements,            int,            MPI_INT)        \
    DO(num_resets,                  int,            MPI_INT)        \
    DO(num_mpi_calls,               int64_t,        mpi_int64_type) \
    DO(num_omp_parallels,           int64_t,        mpi_int64_type) \
    DO(num_omp_tasks,               int64_t,        mpi_int64_type) \
    DO(num_gpu_runtime_calls,       int64_t,        mpi_int64_type) \
    DO(start_time,                  int64_t,        mpi_int64_type) \
    DO(stop_time,                   int64_t,        mpi_int64_type) \
    DO(elapsed_time,                int64_t,        mpi_int64_type) \
    DO(useful_time,                 int64_t,        mpi_int64_type) \
    DO(mpi_time,                    int64_t,        mpi_int64_type) \
    DO(mpi_worker_idle_time,        int64_t,        mpi_int64_type) \
    DO(omp_load_imbalance_time,     int64_t,        mpi_int64_type) \
    DO(omp_scheduling_time,         int64_t,        mpi_int64_type) \
    DO(omp_serialization_time,      int64_t,        mpi_int64_type) \
    DO(gpu_runtime_time,            int64_t,        mpi_int64_type) \
    DO(gpu_useful_time,             int64_t,        mpi_int64_type) \
    DO(gpu_communication_time,      int64_t,        mpi_int64_type) \
    DO(gpu_inactive_time,           int64_t,        mpi_int64_type) \
    DO(_data,                       void*,          address_type)

#define FOR_DLB_MONITOR_PRINTABLE_FIELDS(DO, DO_LAST)                                   \
    DO(num_cpus,                    int,            numCpus,                "%d")       \
    DO(num_omp_threads,             int,            numOmpThreads,          "%d")       \
    DO(cycles,                      int64_t,        cycles,                 "%"PRId64)  \
    DO(instructions,                int64_t,        instructions,           "%"PRId64)  \
    DO(num_measurements,            int,            numMeasurements,        "%d")       \
    DO(num_resets,                  int,            numResets,              "%d")       \
    DO(num_mpi_calls,               int64_t,        numMpiCalls,            "%"PRId64)  \
    DO(num_omp_parallels,           int64_t,        numOmpParallels,        "%"PRId64)  \
    DO(num_omp_tasks,               int64_t,        numOmpTasks,            "%"PRId64)  \
    DO(num_gpu_runtime_calls,       int64_t,        numGpuRuntimeCalls,     "%"PRId64)  \
    DO(elapsed_time,                int64_t,        elapsedTime,            "%"PRId64)  \
    DO(useful_time,                 int64_t,        usefulTime,             "%"PRId64)  \
    DO(mpi_time,                    int64_t,        mpiTime,                "%"PRId64)  \
    DO(mpi_worker_idle_time,        int64_t,        mpiWorkerIdleTime,      "%"PRId64)  \
    DO(omp_load_imbalance_time,     int64_t,        ompLoadImbalanceTime,   "%"PRId64)  \
    DO(omp_scheduling_time,         int64_t,        ompSchedulingTime,      "%"PRId64)  \
    DO(omp_serialization_time,      int64_t,        ompSerializationTime,   "%"PRId64)  \
    DO(gpu_runtime_time,            int64_t,        gpuRuntimeTime,         "%"PRId64)  \
    DO(gpu_useful_time,             int64_t,        gpuUsefulTime,          "%"PRId64)  \
    DO_LAST(gpu_communication_time, int64_t,        gpuCommunicationTime,   "%"PRId64)

/* dlb_pop_metrics_t: per-app aggregated metrics */
/* Note: char name[DLB_MONITOR_NAME_MAX] not included */
#define FOR_DLB_POP_METRICS_FIELDS(DO, DO_LAST)                                             \
    DO(num_cpus,                        int,        numCpus,                    "%d")       \
    DO(num_omp_threads,                 int,        numOmpThreads,              "%d")       \
    DO(num_mpi_ranks,                   int,        numMpiRanks,                "%d")       \
    DO(num_nodes,                       int,        numNodes,                   "%d")       \
    DO(avg_cpus,                        float,      avgCpus,                    "%.1f")     \
    DO(num_gpus,                        int,        numGpus,                    "%d")       \
    DO(cycles,                          double,     cycles,                     "%.0f")     \
    DO(instructions,                    double,     instructions,               "%.0f")     \
    DO(num_measurements,                int64_t,    numMeasurements,            "%"PRId64)  \
    DO(num_mpi_calls,                   int64_t,    numMpiCalls,                "%"PRId64)  \
    DO(num_omp_parallels,               int64_t,    numOmpParallels,            "%"PRId64)  \
    DO(num_omp_tasks,                   int64_t,    numOmpTasks,                "%"PRId64)  \
    DO(num_gpu_runtime_calls,           int64_t,    numGpuRuntimeCalls,         "%"PRId64)  \
    DO(elapsed_time,                    int64_t,    elapsedTime,                "%"PRId64)  \
    DO(useful_time,                     int64_t,    usefulTime,                 "%"PRId64)  \
    DO(mpi_time,                        int64_t,    mpiTime,                    "%"PRId64)  \
    DO(mpi_worker_idle_time,            int64_t,    mpiWorkerIdleTime,          "%"PRId64)  \
    DO(omp_load_imbalance_time,         int64_t,    ompLoadImbalanceTime,       "%"PRId64)  \
    DO(omp_scheduling_time,             int64_t,    ompSchedulingTime,          "%"PRId64)  \
    DO(omp_serialization_time,          int64_t,    ompSerializationTime,       "%"PRId64)  \
    DO(gpu_runtime_time,                int64_t,    gpuRuntimeTime,             "%"PRId64)  \
    DO(min_mpi_normd_proc,              double,     minMpiNormdProc,            "%.0f")     \
    DO(min_mpi_normd_node,              double,     minMpiNormdNode,            "%.0f")     \
    DO(gpu_useful_time,                 int64_t,    gpuUsefulTime,              "%"PRId64)  \
    DO(gpu_communication_time,          int64_t,    gpuCommunicationTime,       "%"PRId64)  \
    DO(gpu_inactive_time,               int64_t,    gpuInactiveTime,            "%"PRId64)  \
    DO(max_gpu_useful_time,             int64_t,    maxGpuUsefulTime,           "%"PRId64)  \
    DO(max_gpu_active_time,             int64_t,    maxGpuActiveTime,           "%"PRId64)  \
    DO(parallel_efficiency,             float,      parallelEfficiency,         "%.2f")     \
    DO(mpi_parallel_efficiency,         float,      mpiParallelEfficiency,      "%.2f")     \
    DO(mpi_communication_efficiency,    float,      mpiCommunicationEfficiency, "%.2f")     \
    DO(mpi_load_balance,                float,      mpiLoadBalance,             "%.2f")     \
    DO(mpi_load_balance_in,             float,      mpiLoadBalanceIn,           "%.2f")     \
    DO(mpi_load_balance_out,            float,      mpiLoadBalanceOut,          "%.2f")     \
    DO(omp_parallel_efficiency,         float,      ompParallelEfficiency,      "%.2f")     \
    DO(omp_load_balance,                float,      ompLoadBalance,             "%.2f")     \
    DO(omp_scheduling_efficiency,       float,      ompSchedulingEfficiency,    "%.2f")     \
    DO(omp_serialization_efficiency,    float,      ompSerializationEfficiency, "%.2f")     \
    DO(device_offload_efficiency,       float,      deviceOffloadEfficiency,    "%.2f")     \
    DO(gpu_parallel_efficiency,         float,      gpuParallelEfficiency,      "%.2f")     \
    DO(gpu_load_balance,                float,      gpuLoadBalance,             "%.2f")     \
    DO(gpu_communication_efficiency,    float,      gpuCommunicationEfficiency, "%.2f")     \
    DO_LAST(gpu_orchestration_efficiency, float,    gpuOrchestrationEfficiency, "%.2f")

/* dlb_node_metrics_t: per-node aggregated metrics */
/* Note: char name[DLB_MONITOR_NAME_MAX] not included */
#define FOR_DLB_NODE_METRICS_FIELDS(DO)             \
    DO(node_id,                         int)        \
    DO(processes_per_node,              int)        \
    DO(total_useful_time,               int64_t)    \
    DO(total_mpi_time,                  int64_t)    \
    DO(max_useful_time,                 int64_t)    \
    DO(max_mpi_time,                    int64_t)    \
    DO(parallel_efficiency,             float)      \
    DO(communication_efficiency,        float)      \
    DO(load_balance,                    float)

#endif /* TALP_TYPES_H */
