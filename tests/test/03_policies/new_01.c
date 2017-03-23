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
    test_generator="gens/basic-generator -a --mode=polling|--mode=async"
    test_generator_ENV=( "LB_TEST_MODE=single" )
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
#include "support/mask_utils.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

static subprocess_descriptor_t spd1;
static subprocess_descriptor_t spd2;
static cpu_set_t sp1_mask;
static cpu_set_t sp2_mask;
static interaction_mode_t mode;

/* Subprocess 1 callbacks */
static void sp1_cb_enable_cpu(int cpuid) {
    CPU_SET(cpuid, &sp1_mask);
}

static void sp1_cb_disable_cpu(int cpuid) {
    CPU_CLR(cpuid, &sp1_mask);
    if (mode == MODE_ASYNC) {
        new_ReturnCpu(&spd1, cpuid);
    }
}

/* Subprocess 2 callbacks */
static void sp2_cb_enable_cpu(int cpuid) {
    CPU_SET(cpuid, &sp2_mask);
}

static void sp2_cb_disable_cpu(int cpuid) {
    CPU_CLR(cpuid, &sp2_mask);
    if (mode == MODE_ASYNC) {
        new_ReturnCpu(&spd2, cpuid);
    }
}

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    mu_init();
    if (mu_get_system_size() < 4) {
        mu_testing_set_sys_size(4);
    }

    // Initialize local masks to [1100] and [0011]
    CPU_ZERO(&sp1_mask);
    CPU_SET(0, &sp1_mask);
    CPU_SET(1, &sp1_mask);
    CPU_ZERO(&sp2_mask);
    CPU_SET(2, &sp2_mask);
    CPU_SET(3, &sp2_mask);

    // Subprocess 1 init
    spd1.id = 111;
    options_init(&spd1.options, NULL);
    memcpy(&spd1.process_mask, &sp1_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd1.id, &spd1.process_mask, NULL, NULL) == DLB_SUCCESS);
    assert( shmem_cpuinfo__init(spd1.id, &spd1.process_mask, NULL) == DLB_SUCCESS);
    assert( pm_callback_set(&spd1.pm, dlb_callback_enable_cpu, (dlb_callback_t)sp1_cb_enable_cpu)
            == DLB_SUCCESS);
    assert( pm_callback_set(&spd1.pm, dlb_callback_disable_cpu, (dlb_callback_t)sp1_cb_disable_cpu)
            == DLB_SUCCESS);
    assert( new_Init(&spd1) == DLB_SUCCESS);

    // Subprocess 2 init
    spd2.id = 222;
    options_init(&spd2.options, NULL);
    memcpy(&spd1.process_mask, &sp1_mask, sizeof(cpu_set_t));
    memcpy(&spd2.process_mask, &sp2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd2.id, &spd2.process_mask, NULL, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd2.id, &spd2.process_mask, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd2.pm, dlb_callback_enable_cpu, (dlb_callback_t)sp2_cb_enable_cpu)
            == DLB_SUCCESS );
    assert( pm_callback_set(&spd2.pm, dlb_callback_disable_cpu, (dlb_callback_t)sp2_cb_disable_cpu)
             == DLB_SUCCESS );
    assert( new_Init(&spd2) == DLB_SUCCESS );

    // Get interaction mode
    assert( spd1.options.mode == spd2.options.mode );
    mode = spd1.options.mode;
    if (mode == MODE_ASYNC) {
        assert( shmem_async_init(spd2.id, &spd2.pm, NULL) == DLB_SUCCESS );
        assert( shmem_async_init(spd1.id, &spd1.pm, NULL) == DLB_SUCCESS );
    }

    // Subprocess 1 wants to acquire CPU 3
    assert( new_AcquireCpu(&spd1, 3) == DLB_NOTED );

    // Subprocess 2 no longer needs CPU 3
    CPU_CLR(3, &sp2_mask);
    assert( new_LendCpu(&spd2, 3) == DLB_SUCCESS );

    // Subprocess 1 needs to poll
    if (mode == MODE_POLLING) {
        assert( new_AcquireCpu(&spd1, 3) == DLB_SUCCESS );
    }

    // Poll a certain number of times until mask1 contains CPU 3
    assert_loop( CPU_ISSET(3, &sp1_mask) );

    // Subprocess 2 needs again CPU 3
    assert( new_AcquireCpu(&spd2, 3) == DLB_NOTED );

    // Subprocesses 1 and 2 need to poll
    if (mode == MODE_POLLING) {
        assert( new_ReturnCpu(&spd1, 3) == DLB_SUCCESS );
        assert( new_AcquireCpu(&spd2, 3) == DLB_SUCCESS );
    }

    // Poll a certain number of times until mask2 contains CPU 3, and mask1 don't
    assert_loop( CPU_ISSET(3, &sp2_mask) );
    assert( !CPU_ISSET(3, &sp1_mask) );

    // Subprocess 1 no longer needs CPU 1
    CPU_CLR(1, &sp1_mask);
    assert( new_LendCpu(&spd1, 1) == DLB_SUCCESS );

    // Subprocess 2 needs CPU 1
    assert( new_AcquireCpu(&spd2, 1) == DLB_SUCCESS );

    // Subprocess 1 reclaims
    assert( new_ReclaimCpu(&spd1, 1) == DLB_NOTED );

    // Subprocesses 1 & 2 needs to poll
    if (mode == MODE_POLLING) {
        assert( new_ReturnCpu(&spd2, 1) == DLB_SUCCESS );
        assert( new_AcquireCpu(&spd1, 1) == DLB_SUCCESS );
    }
    assert_loop( CPU_ISSET(1, &sp1_mask) );
    assert( !CPU_ISSET(1, &sp2_mask) );

    // Subprocess 1 lends everything
    assert( new_LendCpu(&spd1, 0) == DLB_SUCCESS );
    assert( new_LendCpu(&spd1, 1) == DLB_SUCCESS );

    if (mode == MODE_ASYNC) {
        // CPU 1 was still requested
        assert_loop( CPU_ISSET(1, &sp2_mask) );

        // Subprocess 2 acquire everything
        assert( new_AcquireCpu(&spd2, 0) == DLB_SUCCESS );
        assert( new_AcquireCpu(&spd2, 1) == DLB_NOUPDT );
    } else {
        // Subprocess 2 acquire everything
        assert( new_AcquireCpu(&spd2, 0) == DLB_SUCCESS );
        assert( new_AcquireCpu(&spd2, 1) == DLB_SUCCESS );
    }

    // Policy finalize
    assert( new_Finish(&spd1) == DLB_SUCCESS );
    assert( new_Finish(&spd2) == DLB_SUCCESS );

    // Async finalize
    if (mode == MODE_ASYNC) {
        assert( shmem_async_finalize(spd2.id) == DLB_SUCCESS );
        assert( shmem_async_finalize(spd1.id) == DLB_SUCCESS );
    }

    // Subprocess 1 shmems finalize
    assert( shmem_cpuinfo__finalize(spd1.id) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd1.id) == DLB_SUCCESS );

    // Subprocess 2 shmems finalize
    assert( shmem_cpuinfo__finalize(spd2.id) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd2.id) == DLB_SUCCESS );

    return 0;
}
