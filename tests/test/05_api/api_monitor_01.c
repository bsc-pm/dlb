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

#include <LB_core/spd.h>
#include <LB_core/DLB_talp.h>
#include <apis/dlb.h>
#include "LB_core/spd.h"
#include "unique_shmem.h"

#include <apis/dlb.h>

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

    int err = 0;
    char* name = "Test Region";

    dlb_monitor_t* test1 = DLB_MonitoringRegionRegister(name);

    monitor_info_t* check1 = (monitor_info_t*) test1;
    double tmp_mpi,tmp_comp;
    double tmp2_mpi,tmp2_comp;

    tmp_mpi = to_secs(check1->mpi_time);
    tmp_comp = to_secs(check1->compute_time);

    err = DLB_MonitoringRegionStart(test1);
    assert(err == 0);
    sleep(1);
    err = DLB_MonitoringRegionStop(test1);
    assert(err == 0);

    tmp2_mpi = to_secs(check1->mpi_time);
    tmp2_comp = to_secs(check1->compute_time);

    assert(tmp_comp != tmp2_comp);
    assert(tmp_mpi == tmp2_mpi);

    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
