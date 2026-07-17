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

#ifndef DLB_CORE_MNGO_H
#define DLB_CORE_MNGO_H

#include "LB_comm/shmem_barrier.h"
#include "LB_core/spd.h"
#include "mngo/mngo_resources.h"
#ifdef MPI_LIB
# include "mpi/mpi_core.h"
#endif

#include "apis/dlb_mngo.h"
#include "mngo/mngo_regions.h"

#include <pthread.h>

/**
 * This structure holds the information necessary for MNGO to work.
 */
typedef struct DLB_mngo_info_t {
    // The manager id / Or MngoID
    size_t mid;

    // The MPI rank (0 if MPI not abailable)
    int rank;

    // Shared memory barrier id
    int barrier_id;

#ifdef MPI_LIB
    // MPI Communicator for MNGO
    MPI_Comm comm;
#endif

    barrier_t *dlb_node_barrier;

    // The manager pthread
    pthread_t thread;

    mngo_state_t default_state;

    mngo_regions_manager_t region_handler;

    // MNGO Configuration
    float lb_in_threshold;
    float lb_out_threshold;
} mngo_info_t;

void mngo_init(subprocess_descriptor_t *spd);

void mngo_fini(const subprocess_descriptor_t *spd);

/*
 * To determine if the regions is begining ending or restarting, we have an
 * enum with all possible values.
 */
typedef enum mngo_manager_entrypoint_t {
    MANAGER_BEGIN = 1 << 0,
    MANAGER_END   = 1 << 1,
} mngo_manager_entrypoint_t;

/*
 * The MODULE implements different modes of execution:
 *  - helper-thread
 *  - mpi-collectives (unimplemented)
 *  - regions
 *
 *  All this modes will make use of the mngo_manager function, which has the
 *  main MNGO logic.
 */
int mngo_manager(subprocess_descriptor_t *spd, dlb_mngo_region_t *region,
                  mngo_manager_entrypoint_t entrypoint);

/**
 * Wakes the manager.
 *
 * Note: All the processes of the node must exeucte the routine, in order to
 * work.
 */
int mngo_manager_wake(const subprocess_descriptor_t *spd);

/**
 * Routine used only for testing, to manully force a change in the
 * process mask from mngo.
 */
int mngo_test_drom(subprocess_descriptor_t *spd, int ncpu);

/**
 *
 */
dlb_mngo_region_t * mngo_region_register(const subprocess_descriptor_t *spd, const char *name);

/**
 *
 */
int mngo_region_start(subprocess_descriptor_t *spd, dlb_mngo_region_t *region);

/**
 *
 */
int mngo_region_stop(subprocess_descriptor_t *spd, dlb_mngo_region_t *region);

#endif
