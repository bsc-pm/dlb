/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "assert_noshm.h"

#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"

#include <assert.h>

/* Test message queue */

static int num_cb_called = 0;
static void cb_enable_cpu(int cpuid) { ++num_cb_called; }
static void cb_disable_cpu(int cpuid) { ++num_cb_called; }

int main(int argc, char **argv) {
    pm_interface_t pm = {
        .dlb_callback_enable_cpu_ptr = cb_enable_cpu,
        .dlb_callback_disable_cpu_ptr = cb_disable_cpu
    };
    pid_t pid = 42;

    assert( shmem_async_init(pid, &pm, NULL) == DLB_SUCCESS );
    shmem_async_enable_cpu(pid, 1);
    shmem_async_disable_cpu(pid, 1);
    assert( shmem_async_finalize(pid) == DLB_SUCCESS );
    assert( num_cb_called == 2 );

    return 0;
}
