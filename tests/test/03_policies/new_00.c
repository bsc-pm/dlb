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

#include "assert_loop.h"
#include "assert_noshm.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_policies/new.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>

static volatile int enable_times = 0;
static volatile int disable_times = 0;

static void cb_enable_cpu(int cpuid) {
    __sync_add_and_fetch(&enable_times, 1);
}

static void cb_disable_cpu(int cpuid) {
    __sync_add_and_fetch(&disable_times, 1);
}

int main( int argc, char **argv ) {
    subprocess_descriptor_t spd;
    spd.id = getpid();
    spd.options.mode = MODE_ASYNC;
    sched_getaffinity(0, sizeof(cpu_set_t), &spd.process_mask);

    // Initialize shmems and callbacks
    assert( shmem_procinfo__init(spd.id, &spd.process_mask, NULL, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd.id, &spd.process_mask, NULL) == DLB_SUCCESS );
    assert( shmem_async_init(spd.id, &spd.pm, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd.pm, dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu)
            == DLB_SUCCESS );
    assert( pm_callback_set(&spd.pm, dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu)
            == DLB_SUCCESS );

    // Init
    assert( new_Init(&spd) == DLB_SUCCESS );

    // Reclaim a non-lent CPU (this should not trigger a callback)
    assert( new_ReclaimCpu(&spd, 0) == DLB_NOUPDT );

    // Lend (this should not trigger a callback)
    assert( new_LendCpu(&spd, 0) == DLB_SUCCESS );

    // Reclaim a lent CPU (callback enable)
    assert( new_ReclaimCpu(&spd, 0) == DLB_SUCCESS );

    // Lend
    assert( new_LendCpu(&spd, 0) == DLB_SUCCESS );

    // Acquire a lent CPU (callback enable)
    assert( new_AcquireCpu(&spd, 0) == DLB_SUCCESS );

    // Finalize
    assert( new_Finish(&spd) == DLB_SUCCESS );

    // Poll a limited number of times if at least enable_callback was called once
    assert_loop( enable_times > 0 );

    // Finalize shmems
    assert( shmem_async_finalize(spd.id) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd.id) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd.id, false) == DLB_SUCCESS );

    return 0;
}
