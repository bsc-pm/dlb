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
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_core/DLB_talp.h"
#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/options.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

/* Test simple TALP & Monitoring Regions functionalities */

enum { USLEEP_TIME = 1000 };

int main(int argc, char *argv[]) {

    enum { NUM_MEASUREMENTS = 10 };
    int i, j;

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --talp-papi --shm-key=";
    strcat(options, SHMEM_KEY);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);
    memcpy(&spd.process_mask, &process_mask, sizeof(cpu_set_t));
    talp_init(&spd);

    talp_info_t *talp_info = spd.talp_info;
    dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;

    /* Register custom monitoring region */
    dlb_monitor_t *monitor = monitoring_region_register(&spd, "Test monitor");
    assert( monitor != NULL );

    /* Test MPI + nested user defined region */
    {
        /* TALP initial values */
        assert( mpi_monitor->accumulated_MPI_time == 0 );
        assert( mpi_monitor->accumulated_computation_time == 0 );
        assert( monitoring_region_get_MPI_region(&spd) == mpi_monitor );

        /* Start MPI monitor */
        talp_mpi_init(&spd);

        /* Start and Stop custom monitoring region */
        monitoring_region_start(&spd, monitor);
        monitoring_region_stop(&spd, monitor);

        /* Stop MPI monitor */
        talp_mpi_finalize(&spd);

        /* Test MPI monitor values are correct and greater than custom monitor */
        assert( mpi_monitor->accumulated_MPI_time == 0 );
        assert( mpi_monitor->accumulated_computation_time > 0 );
        assert( mpi_monitor->accumulated_computation_time > monitor->accumulated_computation_time );
        assert( mpi_monitor->elapsed_time > monitor->elapsed_time );
#ifdef PAPI_LIB
        assert( mpi_monitor->accumulated_instructions > monitor->accumulated_instructions );
        assert( mpi_monitor->accumulated_cycles > monitor->accumulated_cycles );
#endif

        /* Test custom monitor values */
        assert( monitor->num_measurements == 1 );
        assert( monitor->accumulated_MPI_time == 0 );
        assert( monitor->accumulated_computation_time != 0 );
        assert( monitoring_region_reset(&spd, monitor) == DLB_SUCCESS );
        assert( monitor->num_resets == 1 );
        assert( monitor->accumulated_MPI_time == 0 );
        assert( monitor->accumulated_computation_time == 0 );
#ifdef PAPI_LIB
        assert( monitor->accumulated_instructions == 0 );
        assert( monitor->accumulated_cycles == 0 );
#endif
    }

    /* Test that reset/start/stop/report may also be invoked with the implicit DLB_MPI_REGION */
    {
        /* Reset MPI monitoring region */
        assert( monitoring_region_reset(&spd, DLB_MPI_REGION) == DLB_SUCCESS );
        assert( !monitoring_region_is_started(mpi_monitor) );
        assert( mpi_monitor->elapsed_time == 0 );
        assert( mpi_monitor->num_measurements == 0 );

        /* Start MPI monitor */
        talp_mpi_init(&spd);

        /* Stop MPI monitoring region */
        assert( monitoring_region_stop(&spd, DLB_MPI_REGION) == DLB_SUCCESS );
        assert( !monitoring_region_is_started(mpi_monitor) );
        assert( mpi_monitor->elapsed_time > 0 );
        assert( mpi_monitor->num_measurements == 1 );

        /* Start MPI monitoring region */
        assert( monitoring_region_start(&spd, DLB_MPI_REGION) == DLB_SUCCESS );
        assert( monitoring_region_is_started(mpi_monitor) );

        /* Stop MPI monitor */
        talp_mpi_finalize(&spd);

        /* Report MPI region */
        assert( monitoring_region_report(&spd, DLB_MPI_REGION) == DLB_SUCCESS );
    }

    /* Test number of measurements */
    {
        for (i=0; i<NUM_MEASUREMENTS; ++i) {
            monitoring_region_start(&spd, monitor);
            monitoring_region_stop(&spd, monitor);
        }
        assert( monitor->num_measurements == NUM_MEASUREMENTS );
        int64_t last_elapsed_measurement = monitor->stop_time - monitor->start_time;
        assert( last_elapsed_measurement > 0 );
        assert( last_elapsed_measurement * NUM_MEASUREMENTS/10 < monitor->elapsed_time );
    }

    /* Test invalid start/stop call order */
    {
        assert( monitoring_region_stop(&spd, monitor) == DLB_NOUPDT );
        assert( monitoring_region_start(&spd, monitor) == DLB_SUCCESS );
        assert( monitoring_region_start(&spd, monitor) == DLB_NOUPDT );
        assert( monitoring_region_stop(&spd, monitor) == DLB_SUCCESS );
        assert( monitoring_region_stop(&spd, monitor) == DLB_NOUPDT );
    }

    /* Test nested regions */
    {
        dlb_monitor_t *monitor1 = monitoring_region_register(&spd, "Test nested 1");
        dlb_monitor_t *monitor2 = monitoring_region_register(&spd, "Test nested 2");
        dlb_monitor_t *monitor3 = monitoring_region_register(&spd, "Test nested 3");
        monitoring_region_start(&spd, monitor1);
        usleep(USLEEP_TIME);
        for (i=0; i<NUM_MEASUREMENTS; ++i) {
            monitoring_region_start(&spd, monitor2);
            usleep(USLEEP_TIME);
            for (j=0; j<NUM_MEASUREMENTS; ++j) {
                monitoring_region_start(&spd, monitor3);
                usleep(USLEEP_TIME);
                monitoring_region_stop(&spd, monitor3);
            }
            usleep(USLEEP_TIME);
            monitoring_region_stop(&spd, monitor2);
        }
        usleep(USLEEP_TIME);
        monitoring_region_stop(&spd, monitor1);
        assert( monitor1->num_measurements == 1 );
        assert( monitor2->num_measurements == NUM_MEASUREMENTS );
        assert( monitor3->num_measurements == NUM_MEASUREMENTS * NUM_MEASUREMENTS );
        assert( monitor1->elapsed_time > monitor2->elapsed_time );
        assert( monitor2->elapsed_time > monitor3->elapsed_time );
    }

    /* Test monitor register with repeated or NULL names */
    {
        dlb_monitor_t *monitor3 = monitoring_region_register(&spd, "Test nested 3");
        dlb_monitor_t *monitor4 = monitoring_region_register(&spd, "Test nested 3");
        assert( monitor4 == monitor3 );
        dlb_monitor_t *monitor5 = monitoring_region_register(&spd, NULL);
        assert( monitor5 != NULL );
        dlb_monitor_t *monitor6 = monitoring_region_register(&spd, NULL);
        assert( monitor6 != NULL );
        assert( monitor6 != monitor5 );
        assert( strcmp(monitor5->name, "Anonymous Region 1") == 0 );
        assert( strcmp(monitor6->name, "Anonymous Region 2") == 0 );
    }

    /* Test monitor name length */
    {
        const char *long_name = "This is a very long name. It is so long that"
            " it is more than MONITOR_MAX_KEY_LEN which, at the time of writing,"
            " should be 128 characters.";
        dlb_monitor_t *monitor7 = monitoring_region_register(&spd, long_name);
        dlb_monitor_t *monitor8 = monitoring_region_register(&spd, long_name);
        assert( monitor7 == monitor8 );
        monitoring_region_report(&spd, monitor7);
        monitoring_region_report(&spd, mpi_monitor);
    }

    talp_finalize(&spd);

    return 0;
}
