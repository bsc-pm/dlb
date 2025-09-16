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

#ifndef TALP_TYPES_H
#define TALP_TYPES_H

#include "apis/dlb_types.h"
#include "support/atomic.h"
#include "support/gtree.h"
#include "support/gslist.h"

#include <pthread.h>

/* The structs below are only for private DLB_talp.c use, but they are defined
 * in this header for testing purposes. */

/* The sample contains the temporary per-thread accumulated values of all the
 * measured metrics. Once the sample is flushed, a macrosample is created using
 * samples from all* threads. The sample starts and ends on each one of the
 * following scenarios:
 *  - MPI Init/Finalize
 *  - end of a parallel region
 *  - a region starts or stops
 *  - a request from the API
 *  (*): on nested parallelism, only threads from that parallel region are reduced
 */
typedef struct DLB_ALIGN_CACHE talp_sample_t {
    struct {
        atomic_int_least64_t useful;
        atomic_int_least64_t not_useful_mpi;
        atomic_int_least64_t not_useful_omp_in;
        atomic_int_least64_t not_useful_omp_out;
        atomic_int_least64_t not_useful_gpu;
    } timers;
#if PAPI_LIB
    struct {
        atomic_int_least64_t cycles;
        atomic_int_least64_t instructions;
    } counters;
    // The last_read counters contain the PAPI_Read values from the beginning of the last useful state.
    // This enables to compute the difference without the need to call PAPI_Reset()
    struct {
        atomic_int_least64_t cycles;
        atomic_int_least64_t instructions;
    } last_read_counters;
#endif
    struct {
        atomic_int_least64_t num_mpi_calls;
        atomic_int_least64_t num_omp_parallels;
        atomic_int_least64_t num_omp_tasks;
        atomic_int_least64_t num_gpu_runtime_calls;
    } stats;
    int64_t last_updated_timestamp;
    enum talp_sample_state {
        disabled,
        useful,
        not_useful_mpi,
        not_useful_omp_in,
        not_useful_omp_out,
        not_useful_gpu,
    } state;
} talp_sample_t;

/* Sample for each GPU device */
typedef struct talp_gpu_sample_t {
    struct {
        int64_t useful;
        int64_t communication;
        int64_t inactive;
    } timers;
} talp_gpu_sample_t;

/* The macrosample is a temporary aggregation of all metrics in samples of all,
 * or a subset of, threads. It is only constructed when samples are flushed and
 * aggregated. The macrosample is then used to update all started monitoring
 * regions. */
typedef struct talp_macrosample_t {
    struct {
        int64_t useful;
        int64_t not_useful_mpi;
        int64_t not_useful_omp_in_lb;
        int64_t not_useful_omp_in_sched;
        int64_t not_useful_omp_out;
        int64_t not_useful_gpu;
    } timers;
    struct {
        int64_t useful;
        int64_t communication;
        int64_t inactive;
    } gpu_timers;
#ifdef PAPI_LIB
    struct {
        int64_t cycles;
        int64_t instructions;
    } counters;
#endif
    struct {
        int64_t num_mpi_calls;
        int64_t num_omp_parallels;
        int64_t num_omp_tasks;
        int64_t num_gpu_runtime_calls;
    } stats;
} talp_macrosample_t;

/* Talp info per spd */
typedef struct talp_info_t {
    struct {
        bool have_shmem:1;          /* whether to record data in shmem */
        bool have_minimal_shmem:1;  /* whether to create a shmem for the global region */
        bool external_profiler:1;   /* whether to update shmem on every sample */
        bool papi:1;                /* whether to collect PAPI counters */
        bool have_mpi:1;            /* whether TALP regions have MPI events */
        bool have_openmp:1;         /* whether TALP regions have OpenMP events */
        bool have_gpu:1;            /* whether TALP regions have GPU events */
    } flags;
    int             ncpus;          /* Number of process CPUs (also num samples) */
    dlb_monitor_t   *monitor;       /* Convenience pointer to the global region */
    GTree           *regions;       /* Tree of monitoring regions */
    GSList          *open_regions;  /* List of open regions */
    pthread_mutex_t regions_mutex;  /* Mutex to protect regions allocation/iteration */
    talp_sample_t   **samples;      /* Per-thread ongoing sample,
                                       added to all monitors when finished */
    pthread_mutex_t samples_mutex;  /* Mutex to protect samples allocation/iteration */
    talp_gpu_sample_t gpu_sample;   /* Per-GPU sample (for now only 1 supported) */
} talp_info_t;

/* Private data per monitor */
typedef struct monitor_data_t {
    int             id;
    int             node_shared_id;         /* id for allocating region in the shmem */
    struct {
        bool started:1;
        bool internal:1;                    /* internal regions are not reported */
        bool enabled:1;
    } flags;
} monitor_data_t;


#endif /* TALP_TYPES_H */
