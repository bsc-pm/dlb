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


#endif /* TALP_TYPES_H */
