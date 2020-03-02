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
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include <apis/dlb_talp.h>
#include <apis/dlb_errors.h>

#include <sched.h>
#include <stdint.h>
#include <assert.h>


typedef struct talp_info_t {
    dlb_monitor_t   mpi_monitor;
    cpu_set_t       workers_mask;
    cpu_set_t       mpi_mask;
} talp_info_t;


int main(int argc, char *argv[]) {

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    spd_enter_dlb(NULL);
    assert( Initialize(thread_spd, 111, 0, &process_mask, options) == DLB_SUCCESS );

    talp_info_t *talp_info = thread_spd->talp_info;

    int64_t mpi_time = talp_info->mpi_monitor.accumulated_MPI_time;
    int64_t computation_time = talp_info->mpi_monitor.accumulated_computation_time;
    assert( mpi_time == 0 && computation_time == 0);

    assert(CPU_COUNT(&talp_info->workers_mask) == 1);
    assert(CPU_COUNT(&talp_info->mpi_mask) == 0);

    assert( Finish(thread_spd) == DLB_SUCCESS );

    return 0;
}
