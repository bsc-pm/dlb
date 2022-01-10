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
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/mytime.h"

#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Test MPI Monitoring Regions with/without LeWI */

enum { USLEEP_TIME = 100000 };

typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;
    cpu_set_t       workers_mask;
    cpu_set_t       mpi_mask;
} talp_info_t;


int main(int argc, char *argv[]) {

    /* Process Mask size can be 1..N */
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);
    memcpy(&spd.process_mask, &process_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd.id, /* preinit_pid */ 0, &spd.process_mask,
            NULL, spd.options.shm_key) == DLB_SUCCESS );
    talp_init(&spd);

    bool lewi = spd.options.lewi;
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
    assert( CPU_COUNT(&talp_info->workers_mask) == 1 );
    assert( CPU_COUNT(&talp_info->mpi_mask) == 0 );

    /* Entering MPI */
    talp_in_mpi(&spd);
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );
    if (lewi) {
        assert( CPU_COUNT(&talp_info->workers_mask) == 0 );
        assert( CPU_COUNT(&talp_info->mpi_mask) == 1 );
    } else {
        assert( CPU_COUNT(&talp_info->workers_mask) == 0 );
        assert( CPU_COUNT(&talp_info->mpi_mask) == 1 );
    }

    /* Leaving MPI */
    talp_out_mpi(&spd);
    assert( mpi_monitor->accumulated_MPI_time != 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );
    assert( CPU_COUNT(&talp_info->workers_mask) == 1 );
    assert( CPU_COUNT(&talp_info->mpi_mask) == 0 );

    /* Checking that the shmem has not been updated (--talp-external-profiler=no) */
    int64_t mpi_time = -1;
    int64_t useful_time = -1;
    assert( shmem_procinfo__gettimes(spd.id, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time == 0 );
    assert( useful_time == 0 );

    int cpuid = sched_getcpu();
    assert( CPU_ISSET(cpuid, &process_mask) );

    /* Disable CPU */
    talp_cpu_disable(&spd, cpuid);
    int64_t time_computation_before = mpi_monitor->accumulated_computation_time;
    usleep(USLEEP_TIME);

    /* Enable CPU */
    talp_cpu_enable(&spd, cpuid);
    int64_t time_computation = mpi_monitor->accumulated_computation_time - time_computation_before;
    assert( time_computation == 0 );

    /* Create a custom monitoring region */
    dlb_monitor_t *monitor = monitoring_region_register("Test");
    monitoring_region_start(&spd, monitor);
    talp_in_mpi(&spd);
    talp_out_mpi(&spd);
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
    assert( shmem_procinfo__gettimes(spd.id, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time == mpi_monitor->accumulated_MPI_time  );
    assert( useful_time == mpi_monitor->accumulated_computation_time );

    spd.options.talp_summary |= SUMMARY_NODE;
    spd.options.talp_summary |= SUMMARY_PROCESS;
    spd.options.talp_summary |= SUMMARY_REGIONS;
    talp_finalize(&spd);
    assert( shmem_procinfo__finalize(spd.id, /* return_stolen */ false, spd.options.shm_key)
            == DLB_SUCCESS );

    return 0;
}
