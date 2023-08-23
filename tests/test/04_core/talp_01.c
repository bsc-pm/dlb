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

/*<testinfo>
    test_generator="gens/basic-generator -a --no-lewi|--lewi"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_core/DLB_talp.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_talp.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/mytime.h"

#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Test MPI Monitoring Regions with/without LeWI */

enum { USLEEP_TIME = 100000 };

int main(int argc, char *argv[]) {

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);

    bool lewi = spd.options.lewi;
    if (lewi) {
        /* FIXME: This test is temporarily disabled with LeWI
         * It needs to be rewritten once LeWI is supported again on TALP
         */
        return 0;
    }

    talp_init(&spd);

    talp_info_t *talp_info = spd.talp_info;
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;

    /* Start MPI monitoring region */
    talp_mpi_init(&spd);
    assert( mpi_monitor->num_measurements == 0 );
    assert( mpi_monitor->num_resets == 0 );
    assert( mpi_monitor->start_time != 0 );
    assert( mpi_monitor->stop_time == 0 );
    assert( mpi_monitor->elapsed_time == 0 );
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time == 0 );
    assert( talp_info->samples[0]->mpi_time == 0 );
    assert( talp_info->samples[0]->useful_time == 0 );
    assert( talp_info->samples[0]->in_useful == true );
    assert( talp_info->samples[0]->cpu_disabled == false );
    assert( talp_info->ncpus == 1 );

    /* We are getting timestamps before and after talp_into_sync_call / talp_out_of_sync_call
     * to check whether sample->last_updated_timestamp is updated. Equivalence
     * is accepted because clock_gettime is allowed to return the same value on
     * successive calls. */
    int64_t time_before;
    int64_t time_after;

    /* Entering MPI, sample is updated, region not yet */
    time_before = get_time_in_ns();
    talp_into_sync_call(&spd, /* is_blocking_collective */ false);
    time_after = get_time_in_ns();
    assert( talp_info->samples[0]->last_updated_timestamp >= time_before
            && talp_info->samples[0]->last_updated_timestamp <= time_after );
    assert( talp_info->samples[0]->mpi_time == 0 );
    assert( talp_info->samples[0]->useful_time > 0 );
    assert( talp_info->samples[0]->in_useful == false );

    /* Leaving MPI */
    time_before = get_time_in_ns();
    talp_out_of_sync_call(&spd, /* is_blocking_collective */ false);
    time_after = get_time_in_ns();
    assert( talp_info->samples[0]->last_updated_timestamp >= time_before
            && talp_info->samples[0]->last_updated_timestamp <= time_after );
    assert( talp_info->samples[0]->mpi_time > 0 );
    assert( talp_info->samples[0]->useful_time > 0 );
    assert( talp_info->samples[0]->in_useful == true );

    /* Update regions */
    assert( monitoring_regions_force_update(&spd) == DLB_SUCCESS );
    assert( mpi_monitor->accumulated_MPI_time > 0 );
    assert( mpi_monitor->accumulated_computation_time > 0 );

    /* Checking that the shmem has not been updated (--talp-external-profiler=no) */
    int64_t mpi_time = -1;
    int64_t useful_time = -1;
    assert( talp_info->external_profiler == false );
    assert( shmem_talp__get_times(0, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time == 0 );
    assert( useful_time == 0 );

    /* Enable --talp-external-profiler and test that the MPI region is updated */
    mpi_time = -1;
    useful_time = -1;
    talp_info->external_profiler = true;
    talp_into_sync_call(&spd, /* is_blocking_collective */ true);
    talp_out_of_sync_call(&spd, /* is_blocking_collective */ true);
    assert( shmem_talp__get_times(0, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time > 0 );
    assert( useful_time > 0 );

    /* Create a custom monitoring region */
    dlb_monitor_t *monitor = monitoring_region_register(&spd, "Test");
    monitoring_region_start(&spd, monitor);
    talp_into_sync_call(&spd, /* is_blocking_collective */ false);
    talp_out_of_sync_call(&spd, /* is_blocking_collective */ false);
    monitoring_region_stop(&spd, monitor);

    /* Finalize MPI */
    talp_mpi_finalize(&spd);
    assert( mpi_monitor->num_measurements == 1 );
    assert( mpi_monitor->num_resets == 0 );
    assert( mpi_monitor->start_time != 0 );
    assert( mpi_monitor->stop_time != 0 );
    assert( mpi_monitor->elapsed_time == mpi_monitor->stop_time - mpi_monitor->start_time );
    assert( mpi_monitor->accumulated_MPI_time != 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );

    /* Checking that the shmem has now been updated */
    mpi_time = -1;
    useful_time = -1;
    assert( shmem_talp__get_times(0, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time == mpi_monitor->accumulated_MPI_time  );
    assert( useful_time == mpi_monitor->accumulated_computation_time );

    spd.options.talp_summary |= SUMMARY_NODE;
    spd.options.talp_summary |= SUMMARY_PROCESS;
    talp_finalize(&spd);

    return 0;
}
