/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef DLB_CORE_TALP_H
#define DLB_CORE_TALP_H

#include "support/mytime.h"
#include "support/types.h"
#include "LB_core/spd.h"

#include <sched.h>
#include <stdbool.h>
#include <pthread.h>

struct talp_loop_struct {
    int iterNum;
    struct timespec initTime;       // Iteration Time
    struct timespec initMPI;        // MPI
    struct timespec partialTime;    // Spent time
    struct timespec partialMPITime; // MPI time
    struct timespec Time;
    struct timespec MPI;
    int depth;
};

typedef struct monitor_s {
    struct timespec tmp_mpi_time;
    struct timespec tmp_compute_time;

    struct timespec mpi_time;
    struct timespec compute_time;

    pthread_spinlock_t talp_lock;
} monitor_t;

typedef struct talp_info_s{
    monitor_t  monitoringRegion;
    cpu_set_t active_working_mask;  // Active CPUs
    cpu_set_t old_working_mask;
    cpu_set_t in_mpi_mask;          // CPUS in MPI
    cpu_set_t active_mpi_mask;      // Active CPUs in mpi

    struct timespec init_app;
    struct timespec end_app;

} talp_info_t;

typedef struct monitor_info_s {
    char* name;
    struct timespec  mpi_time;     // Elapsed
    struct timespec  compute_time; // Elapsed * numCPU

    struct timespec elapsed_time;  // Elapsed time
    struct timespec idle_time;
    struct timespec lend_time;
} monitor_info_t;

/*  Initializes the module structures */
void talp_init( subprocess_descriptor_t* spd);

/*  Finishes the execution of the module and shows the final report if needed */
void talp_finish( subprocess_descriptor_t *spd);

/*  Enables the cpuid for the current process */
void talp_cpu_enable(int cpuid);

/*  Disables the cpuid for the current process */
void talp_cpu_disable(int cpuid);

/*  Update the metrics when entering MPI */
void talp_in_mpi(void);

/*  Update the metrics when going out MPI */
void talp_out_mpi(void);

/*  Update the metrics when entering MPI blocking call */
void talp_in_blocking_call(void);

/*  Update the metrics when going out MPI blocking call */
void talp_out_blocking_call(void);

/*  Function that updates the metrics. */
void talp_update_monitor(monitor_t* region);

/*  Function that updates the metrics specifically for MPI time. */
void talp_update_monitor_mpi(monitor_t* region);

/*  Function that updates the metrics specifically for Compute time. */
void talp_update_monitor_comp(monitor_t* region);

talp_info_t * get_talp_global_info(void);

monitor_t* get_talp_monitoringRegion(void);

double talp_get_mpi_time(void);

double talp_get_compute_time(void);

#ifdef MPI_LIB
/* Initialize necessary structures */
void talp_mpi_init(void);

/* Handler of the Finalize of MPI */
void talp_mpi_finalize(void);

/* Function to print the stats of the process */
void talp_mpi_report(void);
#endif

void monitoring_regions_finalize(void);
int monitoring_region_start(monitor_info_t* p);
int monitoring_region_stop(monitor_info_t* p);
dlb_monitor_t* monitoring_region_register(const char* name);
#endif
