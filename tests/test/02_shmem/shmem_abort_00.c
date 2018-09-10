/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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
    test_exec_fail=yes
</testinfo>*/

#include "LB_comm/shmem_cpuinfo.h"
#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>

void __gcov_flush() __attribute__((weak));

int main(int argc, char **argv) {
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    assert( shmem_cpuinfo__init(getpid(), &process_mask, NULL) == DLB_SUCCESS );
    fatal("This fatal should clean shmems");
    return 0;
}
