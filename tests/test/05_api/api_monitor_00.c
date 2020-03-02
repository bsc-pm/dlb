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

#include <LB_core/DLB_talp.h>
#include <apis/dlb.h>
#include <apis/dlb_talp.h>
#include <support/mytime.h>

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Basic DROM init / pre / post / finalize within 1 process */


int main(int argc, char **argv) {
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);

    char options[64] = " --talp --shm-key=";
    strcat(options, SHMEM_KEY);
    assert( DLB_Init(0, &process_mask, options) == DLB_SUCCESS );

    char value[16];
    assert( DLB_GetVariable("--talp", value) == DLB_SUCCESS );

    char* name = "Test Region";

    dlb_monitor_t *monitor = DLB_MonitoringRegionRegister(name);

    double tmp_mpi,tmp_comp;

    tmp_mpi = nsecs_to_secs(monitor->accumulated_MPI_time);
    tmp_comp = nsecs_to_secs(monitor->accumulated_computation_time);

    assert(tmp_comp == 0);
    assert(tmp_mpi == 0);

    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
