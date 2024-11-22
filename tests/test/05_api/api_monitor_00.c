/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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
#include "apis/dlb.h"
#include "apis/dlb_talp.h"
#include "support/mytime.h"

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Test Monitoring Regions API */

int main(int argc, char **argv) {
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);

    /* Test DLB_ERR_NOTALP */
    {
        char notalp_opts[64] = "--shm-key=";
        strcat(notalp_opts, SHMEM_KEY);
        assert( DLB_Init(0, &process_mask, notalp_opts) == DLB_SUCCESS );
        dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Test monitor");
        assert( monitor == NULL );
        assert( DLB_MonitoringRegionStart(monitor) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionStop(monitor) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionReset(monitor) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionReport(monitor) == DLB_ERR_NOTALP );
        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    /* Test with --talp enabled */
    {
        char options[64] = "--talp --talp-summary=all --shm-key=";
        strcat(options, SHMEM_KEY);
        assert( DLB_Init(0, &process_mask, options) == DLB_SUCCESS );

        char value[16];
        assert( DLB_GetVariable("--talp", value) == DLB_SUCCESS );
        assert( strcmp(value, "yes") == 0 );

        dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Test monitor");
        assert( monitor != NULL );

        assert( DLB_MonitoringRegionStart(monitor) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStart(monitor) == DLB_NOUPDT );
        assert( DLB_MonitoringRegionStop(monitor) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStop(monitor) == DLB_NOUPDT );
        assert( DLB_MonitoringRegionReset(monitor) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionReport(monitor) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStart(monitor) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStop(monitor) == DLB_SUCCESS );
        assert( monitor->useful_time == monitor->elapsed_time );

        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    /* Test monitoring regions after MPI_Finalize */
    {
        char options[64] = "--talp --shm-key=";
        strcat(options, SHMEM_KEY);
        assert( DLB_Init(0, &process_mask, options) == DLB_SUCCESS );

        /* Create custom monitor */
        dlb_monitor_t *custom_monitor = DLB_MonitoringRegionRegister("Custom monitor");
        assert( DLB_MonitoringRegionStart(custom_monitor) == DLB_SUCCESS );

        /* Simulate MPI_Init + MPI_Barrier + MPI_Finalize */
        bool is_blocking_collective = true;
        assert( DLB_Init(0, NULL, NULL) == DLB_ERR_INIT );
        talp_mpi_init(thread_spd);
        talp_into_sync_call(thread_spd, is_blocking_collective);
        talp_out_of_sync_call(thread_spd, is_blocking_collective);
        talp_mpi_finalize(thread_spd);

        /* Global monitor should still be reachable */
        const dlb_monitor_t *global_monitor = DLB_MonitoringRegionGetGlobal();
        assert( global_monitor != NULL );
        assert( global_monitor->num_measurements == 1 );
        assert( global_monitor->num_mpi_calls == 3 );
        assert( global_monitor->mpi_time > 0 );
        assert( global_monitor->useful_time > 0 );

        /* A region named "Global" (case-insensitive) is equivalent to the global region */
        assert( DLB_MonitoringRegionRegister("Global") == global_monitor );
        assert( DLB_MonitoringRegionRegister("global") == global_monitor );
        assert( DLB_MonitoringRegionRegister("GLOBAL") == global_monitor );

        /* Custom monitor should still be reachable too */
        assert( DLB_MonitoringRegionStop(custom_monitor) == DLB_SUCCESS );
        assert( custom_monitor->num_measurements == 1 );
        assert( custom_monitor->num_mpi_calls == 3 );
        assert( custom_monitor->mpi_time > 0 );
        assert( custom_monitor->useful_time > 0 );
        assert( custom_monitor->elapsed_time > global_monitor->elapsed_time );

        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    return 0;
}
