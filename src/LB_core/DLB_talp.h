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

#include "apis/dlb_talp.h"
#include "support/atomic.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>

/* The structs below are only for private DLB_talp.c use, but they are defined
 * in this header for testing purposes. */

/* The sample contains the temporary per-thread accumulated values of all the
 * measured metrics. Once the sample ends, a macrosample is created using
 * samples from all threads. The sample starts and ends on each one of the
 * following scenarios:
 *  - MPI Init/Finalize
 *  - a region starts or stops
 *  - a request from the API
 */
typedef struct DLB_ALIGN_CACHE talp_sample_t {
    /* Sample accumulated values */
    atomic_int_least64_t    mpi_time;
    atomic_int_least64_t    useful_time;
    atomic_int_least64_t    num_mpi_calls;
#if PAPI_LIB
    atomic_int_least64_t    instructions;
    atomic_int_least64_t    cycles;
#endif
    /* Sample temporary values */
    int64_t     last_updated_timestamp;
    bool        in_useful;
    bool        cpu_disabled;
} talp_sample_t;

/* Talp info per spd */
typedef struct talp_info_t {
    dlb_monitor_t       mpi_monitor;        /* monitor MPI_Init -> MPI_Finalize */
    bool                external_profiler;  /* whether to update shmem on every sample */
    int                 ncpus;              /* Number of process CPUs */
    talp_sample_t       **samples;          /* Per-thread ongoing sample,
                                               added to all monitors when finished */
    pthread_mutex_t     samples_mutex;      /* Mutex to protect samples allocation/iteration */
} talp_info_t;


struct SubProcessDescriptor;

/*  Initializes the module structures */
void talp_init(struct SubProcessDescriptor *spd);

/*  Finalizes the execution of the module and shows the final report if needed */
void talp_finalize(struct SubProcessDescriptor *spd);

/* Start MPI monitoring region */
void talp_mpi_init(const struct SubProcessDescriptor *spd);

/* Stop MPI monitoring region */
void talp_mpi_finalize(const struct SubProcessDescriptor *spd);

/*  Update the metrics when entering MPI */
void talp_in_mpi(const struct SubProcessDescriptor *spd, bool is_blocking_collective);

/*  Update the metrics when going out MPI */
void talp_out_mpi(const struct SubProcessDescriptor *spd, bool is_blocking_collective);

/* Obtain the implicit MPI region */
const struct dlb_monitor_t* monitoring_region_get_MPI_region(
        const struct SubProcessDescriptor *spd);

struct dlb_monitor_t* monitoring_region_register(
        const struct SubProcessDescriptor *spd, const char* name);
int monitoring_region_reset(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_start(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor);
int monitoring_region_stop(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor);
bool monitoring_region_is_started(const struct dlb_monitor_t *monitor);
int monitoring_region_report(const struct SubProcessDescriptor *spd,
        const struct dlb_monitor_t *monitor);
int monitoring_regions_force_update(const struct SubProcessDescriptor *spd);

int talp_collect_pop_metrics(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor, struct dlb_pop_metrics_t *pop_metrics);

int talp_collect_pop_node_metrics(const struct SubProcessDescriptor *spd,
        struct dlb_monitor_t *monitor, struct dlb_node_metrics_t *node_metrics);

#endif
