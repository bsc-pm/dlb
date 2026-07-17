
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

#ifndef _DLB_MNGO_TALP_H
#define _DLB_MNGO_TALP_H

// For subprocess_descriptor_t
#include "LB_core/spd.h"
// For dlb_monitor_t
#include "apis/dlb_talp.h"

typedef struct {
    dlb_monitor_t *monitor;
} mngo_talp_region_t;

typedef struct {
    /* The parallel efficiency of the process */
    float self_parallel_efficiency;
    /* The MPI parallel efficiency of the process */
    float self_mpi_parallel_efficiency;
    /* The OMP parallel efficiency of the process */
    float self_omp_parallel_efficiency;

    /* The parallel efficiency of the node */
    float shared_parallel_efficiency;
    /* The MPI parallel efficiency of the node */
    float shared_mpi_parallel_efficiency;
    /* The OMP parallel efficiency of the node */
    float shared_omp_parallel_efficiency;

    /* The applications global parallel efficiency */
    float global_parallel_efficiency;
    /* The applications global MPI parallel efficiency */
    float global_mpi_parallel_efficiency;
    /* The applications global OMP parallel efficiency */
    float global_omp_parallel_efficiency;

    /* The POP metrics collected for periodic monitoring */
    dlb_pop_metrics_t pop_metrics;
} mngo_talp_metrics_t;

int mngo_talp__region_register(const subprocess_descriptor_t *spd, const char* name, mngo_talp_region_t *region);

const char* mngo_talp__get_region_name(mngo_talp_region_t *region);

int mngo_talp__reset_region(const subprocess_descriptor_t *spd, mngo_talp_region_t *region);

void mngo_talp__take_metrics(const subprocess_descriptor_t *spd, mngo_talp_region_t *region, mngo_talp_metrics_t *metrics);

#endif // _DLB_MNGO_TALP_H
