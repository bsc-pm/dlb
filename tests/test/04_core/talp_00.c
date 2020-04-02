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
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_core/DLB_talp.h"
#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/options.h"

#include <sched.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

/* Test simple TALP & Monitoring Regions functionalities */

enum { USLEEP_TIME = 1000 };

typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;
    cpu_set_t       workers_mask;
    cpu_set_t       mpi_mask;
} talp_info_t;


int main(int argc, char *argv[]) {

    enum { NUM_MEASUREMENTS = 10 };
    int i, j;

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    spd_enter_dlb(NULL);
    options_init(&thread_spd->options, options);
    thread_spd->id = 111;
    memcpy(&thread_spd->process_mask, &process_mask, sizeof(cpu_set_t));
    talp_init(thread_spd);

    talp_info_t *talp_info = thread_spd->talp_info;
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;

    /* TALP initial values */
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time == 0 );
    assert( CPU_COUNT(&talp_info->workers_mask) == 1 );
    assert( CPU_COUNT(&talp_info->mpi_mask) == 0 );

    /* Start and Stop MPI monitor */
    talp_mpi_init();
    usleep(USLEEP_TIME);
    talp_mpi_finalize();

    /* Start and Stop custom monitoring region */
    dlb_monitor_t *monitor = monitoring_region_register("Test monitor");
    assert( monitor != NULL );
    monitoring_region_start(monitor);
    monitoring_region_stop(monitor);

    /* Test MPI monitor values are correct and greater than custom monitor */
    assert( mpi_monitor->accumulated_MPI_time == 0 );
    assert( mpi_monitor->accumulated_computation_time != 0 );
    assert( mpi_monitor->accumulated_computation_time > monitor->accumulated_computation_time );
    assert( mpi_monitor->elapsed_time > monitor->elapsed_time );

    /* Test custom monitor values */
    assert( monitor->num_measurements == 1 );
    assert( monitor->accumulated_MPI_time == 0 );
    assert( monitor->accumulated_computation_time != 0 );
    monitoring_region_reset(monitor);
    assert( monitor->num_resets == 1 );
    assert( monitor->accumulated_MPI_time == 0 );
    assert( monitor->accumulated_computation_time == 0 );

    /* Test number of measurements */
    for (i=0; i<NUM_MEASUREMENTS; ++i) {
        monitoring_region_start(monitor);
        monitoring_region_stop(monitor);
    }
    assert( monitor->num_measurements == NUM_MEASUREMENTS );
    int64_t last_elapsed_measurement = monitor->stop_time - monitor->start_time;
    assert( last_elapsed_measurement > 0 );
    assert( last_elapsed_measurement * NUM_MEASUREMENTS/10 < monitor->elapsed_time );

    /* Test invalid start/stop call order */
    assert( monitoring_region_stop(monitor) == DLB_NOUPDT );
    assert( monitoring_region_start(monitor) == DLB_SUCCESS );
    assert( monitoring_region_start(monitor) == DLB_NOUPDT );
    assert( monitoring_region_stop(monitor) == DLB_SUCCESS );
    assert( monitoring_region_stop(monitor) == DLB_NOUPDT );

    /* Test nested regions */
    dlb_monitor_t *monitor1 = monitoring_region_register("Test nested 1");
    dlb_monitor_t *monitor2 = monitoring_region_register("Test nested 2");
    dlb_monitor_t *monitor3 = monitoring_region_register("Test nested 3");
    monitoring_region_start(monitor1);
    usleep(USLEEP_TIME);
    for (i=0; i<NUM_MEASUREMENTS; ++i) {
        monitoring_region_start(monitor2);
        usleep(USLEEP_TIME);
        for (j=0; j<NUM_MEASUREMENTS; ++j) {
            monitoring_region_start(monitor3);
            usleep(USLEEP_TIME);
            monitoring_region_stop(monitor3);
        }
        usleep(USLEEP_TIME);
        monitoring_region_stop(monitor2);
    }
    usleep(USLEEP_TIME);
    monitoring_region_stop(monitor1);
    assert( monitor1->num_measurements == 1 );
    assert( monitor2->num_measurements == NUM_MEASUREMENTS );
    assert( monitor3->num_measurements == NUM_MEASUREMENTS * NUM_MEASUREMENTS );
    assert( monitor1->elapsed_time > monitor2->elapsed_time );
    assert( monitor2->elapsed_time > monitor3->elapsed_time );

    /* Test monitor register with repeated or NULL names */
    dlb_monitor_t *monitor4 = monitoring_region_register("Test nested 3");
    assert( monitor4 == monitor3 );
    dlb_monitor_t *monitor5 = monitoring_region_register(NULL);
    assert( monitor5 != NULL );
    dlb_monitor_t *monitor6 = monitoring_region_register(NULL);
    assert( monitor6 != NULL );
    assert( monitor6 != monitor5 );
    assert( strcmp(monitor5->name, monitor6->name) == 0 );

    talp_finalize(thread_spd);

    return 0;
}
