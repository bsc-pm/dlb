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

#include "mngo/mngo_talp.h"

#include "talp/regions.h"
#include "talp/talp.h"

// For warning0
#include "support/debug.h"
// For DLB_(errors)
#include "apis/dlb_errors.h"

#ifdef MPI_LIB
#include <mpi.h>
#endif

int mngo_talp__region_register(const subprocess_descriptor_t *spd,
                               const char *name, mngo_talp_region_t *region) {
    region->monitor = region_register(spd, name);
    if (region->monitor == NULL) {
        warning0("MNGO could not create TALP monitoring region for: %s", name);
        return DLB_NOUPDT;
    }

    region_set_internal(region->monitor, true);

    return DLB_SUCCESS;
}

const char *mngo_talp__get_region_name(mngo_talp_region_t *region) {
    return region->monitor->name;
}

static void collect_metrics(const subprocess_descriptor_t *spd,
                            mngo_talp_region_t *region,
                            mngo_talp_metrics_t *metrics) {
    dlb_monitor_t *monitor = region->monitor;

#ifdef MPI_LIB
    mngo_info_t *mngo_info = spd->mngo_info;
#endif
    {
        int64_t accumulated_total_time =
            monitor->useful_time + monitor->mpi_time;

        metrics->self_parallel_efficiency =
            ((float)monitor->useful_time) / accumulated_total_time;

        metrics->self_mpi_parallel_efficiency =
            ((float)monitor->useful_time) / accumulated_total_time;

        metrics->self_omp_parallel_efficiency =
            ((float)accumulated_total_time) / accumulated_total_time;
    }

    {
        dlb_node_metrics_t shared_metrics;
        talp_collect_pop_node_metrics(spd, monitor, &shared_metrics);

        int64_t shared_total_time =
            shared_metrics.total_useful_time + shared_metrics.total_mpi_time;

        metrics->shared_parallel_efficiency =
            ((float)shared_metrics.total_useful_time) / shared_total_time;

        metrics->shared_mpi_parallel_efficiency =
            ((float)shared_metrics.total_useful_time) /
            (shared_metrics.total_useful_time + shared_metrics.total_mpi_time);

        metrics->shared_omp_parallel_efficiency =
            ((float)shared_metrics.total_useful_time +
             shared_metrics.total_mpi_time) /
            shared_total_time;

#ifdef MPI_LIB
        {
            int64_t total_mpi_time;
            PMPI_Allreduce(&monitor->mpi_time, &total_mpi_time, 1, MPI_INT64_T,
                           MPI_SUM, mngo_info->comm);

            int64_t total_useful_time;

            PMPI_Allreduce(&monitor->useful_time, &total_useful_time, 1,
                           MPI_INT64_T, MPI_SUM, mngo_info->comm);

            int64_t total_time = total_useful_time + total_mpi_time;

            metrics->global_parallel_efficiency =
                ((float)total_useful_time) / total_time;

            metrics->global_mpi_parallel_efficiency =
                ((float)total_useful_time) /
                (total_useful_time + total_mpi_time);

            metrics->global_omp_parallel_efficiency =
                ((float)total_useful_time + total_mpi_time) / (total_time);
        }
#endif /* MPI_LIB */
    }
}

void mngo_talp__take_metrics(const subprocess_descriptor_t *spd,
                             mngo_talp_region_t *region,
                             mngo_talp_metrics_t *metrics) {
    collect_metrics(spd, region, metrics);
    region_reset(spd, region->monitor);
}
