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

#include "LB_core/DLB_talp.h"
#include "LB_core/spd.h"
#include "unique_shmem.h"

#include <apis/dlb.h>
#include <apis/dlb_drom.h>

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


#include <pthread.h>
#include <assert.h>

int main(int argc, char *argv[]) {

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);
    assert( DLB_Init(0, &process_mask, options) == DLB_SUCCESS );

    double tmp1,tmp2;

    tmp1 = talp_get_mpi_time();
    tmp2 = talp_get_compute_time();
    assert( tmp1 == 0 && tmp2 == 0);

    const subprocess_descriptor_t* spd = thread_spd;
    talp_info_t* talp_info = (talp_info_t*) spd->talp_info;
    assert(CPU_COUNT(&talp_info->active_working_mask) == 1);
    assert(CPU_COUNT(&talp_info->in_mpi_mask) == 0);
    assert(CPU_COUNT(&talp_info->active_mpi_mask) == 0);

    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
