/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/mytime.h"

#include <sched.h>
#include <stdint.h>
#include <assert.h>

/* Test MPI Monitoring Regions with/without LeWI */

enum { USLEEP_TIME = 100000 };

typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;
    cpu_set_t       workers_mask;
    cpu_set_t       mpi_mask;
} talp_info_t;

typedef struct monitor_data_t {
    bool    started;
    int64_t sample_start_time;
} monitor_data_t;


int main(int argc, char *argv[]) {

    /* Process Mask size can be 1..N */
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    unsigned int process_mask_size = CPU_COUNT(&process_mask);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    spd_enter_dlb(NULL);
    options_init(&thread_spd->options, options);
    thread_spd->id = 111;
    memcpy(&thread_spd->process_mask, &process_mask, sizeof(cpu_set_t));
    talp_init(thread_spd);

    bool lewi = thread_spd->options.lewi;
    talp_info_t *talp_info = thread_spd->talp_info;
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;

    /* Start MPI monitoring region */
    talp_mpi_init();
    assert( mpi_monitor->num_measurements == 0 );
    assert( mpi_monitor->num_resets == 0 );
    assert( mpi_monitor->start_time != 0 );
    assert( mpi_monitor->stop_time == 0 );
    assert( mpi_monitor->elapsed_time == 0 );
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time == 0 );
    assert( CPU_COUNT(&talp_info->workers_mask) == CPU_COUNT(&process_mask) );
    assert( CPU_COUNT(&talp_info->mpi_mask) == 0 );

    /* Entering MPI */
    talp_in_mpi();
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );
    if (lewi) {
        assert( CPU_COUNT(&talp_info->workers_mask) == process_mask_size - 1 );
        assert( CPU_COUNT(&talp_info->mpi_mask) == 1 );
    } else {
        assert( CPU_COUNT(&talp_info->workers_mask) == 0 );
        assert( CPU_COUNT(&talp_info->mpi_mask) == process_mask_size );
    }

    /* Leaving MPI */
    talp_out_mpi();
    assert( mpi_monitor->accumulated_MPI_time != 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );
    assert( CPU_COUNT(&talp_info->workers_mask) == process_mask_size );
    assert( CPU_COUNT(&talp_info->mpi_mask) == 0 );

    int cpuid = sched_getcpu();
    assert( CPU_ISSET(cpuid, &process_mask) );

    /* Disable CPU */
    talp_cpu_disable(cpuid);
    int64_t time_computation_before = mpi_monitor->accumulated_computation_time;
    usleep(USLEEP_TIME);

    /* Enable CPU */
    talp_cpu_enable(cpuid);
    int64_t time_computation = mpi_monitor->accumulated_computation_time - time_computation_before;
    assert( time_computation >= USLEEP_TIME * (process_mask_size-1) );

    /* Create a custom monitoring region */
    dlb_monitor_t *monitor = monitoring_region_register("Test");
    monitoring_region_start(monitor);
    talp_in_mpi();
    talp_out_mpi();
    monitoring_region_stop(monitor);

    /* Finalize MPI */
    talp_mpi_finalize();
    assert( mpi_monitor->num_measurements == 1 );
    assert( mpi_monitor->num_resets == 0 );
    assert( mpi_monitor->start_time != 0 );
    assert( mpi_monitor->stop_time != 0 );
    assert( mpi_monitor->elapsed_time == mpi_monitor->stop_time - mpi_monitor->start_time );
    assert( mpi_monitor->accumulated_MPI_time != 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );

    thread_spd->options.talp_summary |= SUMMARY_NODE;
    thread_spd->options.talp_summary |= SUMMARY_PROCESS;
    thread_spd->options.talp_summary |= SUMMARY_REGIONS;
    talp_finalize(thread_spd);

    return 0;
}
