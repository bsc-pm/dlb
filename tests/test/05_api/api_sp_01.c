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
    test_generator="gens/basic-generator -a --mode=polling|--mode=async"
</testinfo>*/

#include "assert_loop.h"
#include "unique_shmem.h"

#include "apis/dlb_sp.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Same test as 03_policies/lewi_mask_01.c but using the API (sp) */

static dlb_handler_t handler1;
static dlb_handler_t handler2;
static cpu_set_t sp1_mask;
static cpu_set_t sp2_mask;
static interaction_mode_t mode;

/* Subprocess 1 callbacks */
static void sp1_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp1_mask);
}

static void sp1_cb_disable_cpu(int cpuid, void *arg) {
    if (mode == MODE_ASYNC) {
        DLB_ReturnCpu_sp(handler1, cpuid);
    }
    CPU_CLR(cpuid, &sp1_mask);
}

/* Subprocess 2 callbacks */
static void sp2_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp2_mask);
}

static void sp2_cb_disable_cpu(int cpuid, void *arg) {
    if (mode == MODE_ASYNC) {
        DLB_ReturnCpu_sp(handler2, cpuid);
    }
    CPU_CLR(cpuid, &sp2_mask);
}

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Initialize local masks to [1100] and [0011]
    CPU_ZERO(&sp1_mask);
    CPU_SET(0, &sp1_mask);
    CPU_SET(1, &sp1_mask);
    CPU_ZERO(&sp2_mask);
    CPU_SET(2, &sp2_mask);
    CPU_SET(3, &sp2_mask);

    // Initialize constant masks for fast reference
    const cpu_set_t sys_mask = { .__bits = {0xf} };       /* [1111] */
    const cpu_set_t sp1_process_mask = {.__bits={0x3}};   /* [0011] */
    const cpu_set_t sp2_process_mask = {.__bits={0xc}};   /* [1100] */

    // Options
    char options[64] = "--lewi --shm-key=";
    strcat(options, SHMEM_KEY);

    // Subprocess 1 init
    handler1 = DLB_Init_sp(0, &sp1_mask, options);
    assert( handler1 != NULL );
    assert( DLB_CallbackSet_sp(handler1, dlb_callback_enable_cpu,
                (dlb_callback_t)sp1_cb_enable_cpu, NULL) == DLB_SUCCESS);
    assert( DLB_CallbackSet_sp(handler1, dlb_callback_disable_cpu,
                (dlb_callback_t)sp1_cb_disable_cpu, NULL) == DLB_SUCCESS);

    // Subprocess 2 init
    handler2 = DLB_Init_sp(0, &sp2_mask, options);
    assert( handler2 != NULL );
    assert( DLB_CallbackSet_sp(handler2, dlb_callback_enable_cpu,
                (dlb_callback_t)sp2_cb_enable_cpu, NULL) == DLB_SUCCESS );
    assert( DLB_CallbackSet_sp(handler2, dlb_callback_disable_cpu,
                (dlb_callback_t)sp2_cb_disable_cpu, NULL) == DLB_SUCCESS );

    // Get interaction mode (this test is called twice, using mode={polling,async})
    char value[16];
    DLB_GetVariable_sp(handler1, "--mode", value);
    if (strcmp(value, "polling") == 0) {
        mode = MODE_POLLING;
    } else if (strcmp(value, "async") == 0) {
        mode = MODE_ASYNC;
    } else {
        // Unknown value
        return -1;
    }

    int err;

    /* Cpu tests */
    {
        // Subprocess 1 wants to acquire CPU 3
        err = DLB_AcquireCpu_sp(handler1, 3);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );

        // Subprocess 2 no longer needs CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( DLB_LendCpu_sp(handler2, 3) == DLB_SUCCESS );

        // Subprocess 1 may need to poll again */
        if (mode == MODE_POLLING) {
            assert( DLB_AcquireCpu_sp(handler1, 3) == DLB_SUCCESS );
        }

        // Poll a certain number of times until mask1 contains CPU 3
        assert_loop( CPU_ISSET(3, &sp1_mask) );

        // Subprocess 2 needs again CPU 3
        assert( DLB_AcquireCpu_sp(handler2, 3) == DLB_NOTED );

        // Subprocesses 1 and 2 need to poll
        if (mode == MODE_POLLING) {
            assert( DLB_CheckCpuAvailability_sp(handler2, 3) == DLB_NOTED );
            assert( DLB_ReturnCpu_sp(handler1, 3) == DLB_SUCCESS );
            assert( DLB_CheckCpuAvailability_sp(handler2, 3) == DLB_SUCCESS );
        }

        // Poll a certain number of times until mask2 contains CPU 3, and mask1 doesn't
        assert_loop( CPU_ISSET(3, &sp2_mask) );
        assert( !CPU_ISSET(3, &sp1_mask) );

        // Subprocess 1 no longer needs CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( DLB_LendCpu_sp(handler1, 1) == DLB_SUCCESS );

        // Subprocess 2 needs CPU 1
        assert( DLB_AcquireCpu_sp(handler2, 1) == DLB_SUCCESS );

        // Subprocess 1 reclaims
        assert( DLB_ReclaimCpu_sp(handler1, 1) == DLB_NOTED );

        // Subprocesses 1 & 2 needs to poll
        if (mode == MODE_POLLING) {
            assert( DLB_CheckCpuAvailability_sp(handler1, 1) == DLB_NOTED );
            assert( DLB_ReturnCpu_sp(handler2, 1) == DLB_SUCCESS );
            assert( DLB_CheckCpuAvailability_sp(handler1, 1) == DLB_SUCCESS );
        }
        assert_loop( CPU_ISSET(1, &sp1_mask) );
        assert( !CPU_ISSET(1, &sp2_mask) );

        // Test that SP2 receives CPU 1 again if SP1 lends it again
        if (mode == MODE_ASYNC) {
            // Subprocess 1 lends everything
            CPU_ZERO(&sp1_mask);
            assert( DLB_LendCpuMask_sp(handler1, &sys_mask) == DLB_SUCCESS );

            // CPU 1 was still requested by Subprocess 2
            assert_loop( CPU_ISSET(1, &sp2_mask) );

            // Subprocess 2 lends CPU 1
            CPU_CLR(1, &sp2_mask);
            assert( DLB_LendCpu_sp(handler2, 1) == DLB_SUCCESS );

            // Subprocess aquires its mask
            assert( DLB_AcquireCpuMask_sp(handler1, &sp1_process_mask) == DLB_SUCCESS );
            assert_loop( CPU_ISSET(0, &sp1_mask) );
            assert_loop( CPU_ISSET(1, &sp1_mask) );
        }
    }

    /* CpuMask tests */
    {
        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( DLB_LendCpuMask_sp(handler1, &sys_mask) == DLB_SUCCESS );

        // Subprocess 2 acquires everything
        assert( DLB_AcquireCpuMask_sp(handler2, &sys_mask) == DLB_SUCCESS );

        // Subprocess 1 reclaims its CPUs
        assert( DLB_ReclaimCpuMask_sp(handler1, &sp1_process_mask) == DLB_NOTED );

        // Subprocess 1 reclaims everything
        assert( DLB_ReclaimCpuMask_sp(handler1, &sys_mask) == DLB_ERR_PERM );

        // Subprocess 2 lends external CPUs
        CPU_CLR(0, &sp2_mask);
        CPU_CLR(1, &sp2_mask);
        assert( DLB_LendCpuMask_sp(handler2, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocesses 1 needs to poll to set guest field
        if (mode == MODE_POLLING) {
            assert_loop( DLB_CheckCpuAvailability_sp(handler1, 0) == DLB_SUCCESS );
            assert_loop( DLB_CheckCpuAvailability_sp(handler1, 1) == DLB_SUCCESS );
        }

        // Check everyone has their CPUs
        assert_loop( CPU_EQUAL(&sp1_process_mask, &sp1_mask) );
        assert_loop( CPU_EQUAL(&sp2_process_mask, &sp2_mask) );
    }

    /* Cpus tests */
    {
        // Subprocess 1 lends CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( DLB_LendCpu_sp(handler1, 1) == DLB_SUCCESS );

        // Subprocess 2 acquires one CPU
        assert( DLB_AcquireCpus_sp(handler2, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(1, &sp2_mask) );

        // Subprocess 2 acquires one CPU again
        err = DLB_AcquireCpus_sp(handler2, 1);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );

        // Subprocess 1 acquires 1 CPU, it should reclaim CPU 1
        assert( DLB_AcquireCpus_sp(handler1, 1) == DLB_NOTED );
        assert_loop( CPU_ISSET(1, &sp1_mask) );

        // Subprocess 2 returns CPU 1 (async returns by callback)
        if (mode == MODE_POLLING) {
            assert( CPU_ISSET(1, &sp2_mask) );
            CPU_CLR(1, &sp2_mask);
            assert( DLB_ReturnCpu_sp(handler2, 1) == DLB_SUCCESS );
        }

        // Subprocess 1 needs to poll to set guest field
        if (mode == MODE_POLLING) {
            assert_loop( DLB_CheckCpuAvailability_sp(handler1, 1) == DLB_SUCCESS );
        }

        // Subprocess 2 no longer wants external CPUs
        assert( DLB_AcquireCpus_sp(handler2, 0) == DLB_SUCCESS );   /* general queue */
        assert( DLB_LendCpu_sp(handler2, 1) == DLB_SUCCESS );       /* CPU queue */
    }

    /* Reset test */
    {
        // Subprocess 2 lends CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( DLB_LendCpu_sp(handler2, 3) == DLB_SUCCESS );

        // Subprocess 1 acquires one CPU
        assert( DLB_AcquireCpus_sp(handler1, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp1_mask) && CPU_COUNT(&sp1_mask) == 3 );

        // Subprocess 1 decides to shut down CPU 1 without notifying DLB
        CPU_CLR(1, &sp1_mask);

        // Subprocess 1 resets. CPU 1 should not be re-enabled
        assert( DLB_Disable_sp(handler1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(0, &sp1_mask) && CPU_COUNT(&sp1_mask) == 1 );

        // Subprocess 2 recovers CPU 3
        assert( DLB_AcquireCpu_sp(handler2, 3) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp2_mask) );

        // Subprocess 1 enables DLB and CPU 1
        assert( DLB_Enable_sp(handler1) == DLB_SUCCESS );
        CPU_SET(1, &sp1_mask);
    }

    /* MaxParallelism */
    {
        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( DLB_LendCpuMask_sp(handler1, &sys_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows everything
        assert( DLB_Borrow_sp(handler2) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 4
                && CPU_ISSET(0, &sp2_mask) && CPU_ISSET(1, &sp2_mask) );

        // Subprocess 1 acquire its CPUs
        assert( DLB_AcquireCpuMask_sp(handler1, &sp1_process_mask) == DLB_NOTED );

        // Subprocess 2 sets max_parallelism to 2
        DLB_SetMaxParallelism_sp(handler2, 2);

        // Both SPs should have their own CPUs
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                 && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                 && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );
    }

    /* Test CpusInMask */
    /* FIXME: this test has a race condition in mode=async */
    if (mode != MODE_ASYNC)
    {
        // Construct a mask of only 1 CPU from subprocess 1, and all from subprocess 2
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(0, &mask);
        CPU_OR(&mask, &mask, &sp2_mask);

        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( DLB_LendCpuMask_sp(handler1, &sys_mask) == DLB_SUCCESS );

        // Subprocess 2 lends 1 CPU
        CPU_CLR(2, &sp2_mask);
        assert( DLB_LendCpu_sp(handler2, 2) == DLB_SUCCESS );

        // Subprocess 2 tries to acquire 1 CPU from the auxiliar mask [0,2,3] (it should prioritize [2,3])
        assert( DLB_AcquireCpusInMask_sp(handler2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 2 lends 1 CPU, again
        CPU_CLR(2, &sp2_mask);
        assert( DLB_LendCpu_sp(handler2, 2) == DLB_SUCCESS );

        // Subprocess 2 tries to acquire 4 CPUs from the auxiliar mask [0,2,3]
        err = DLB_AcquireCpusInMask_sp(handler2, 4, &mask);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 3
                && CPU_ISSET(0, &sp2_mask) && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 2 removes any previous requests
        if (mode == MODE_ASYNC) {
            assert( DLB_AcquireCpus_sp(handler2, 0) == DLB_SUCCESS );
        }

        // Subprocess 2 lends everything (0 first, and 2-3 later)
        CPU_ZERO(&sp2_mask);
        assert( DLB_LendCpu_sp(handler2, 0) == DLB_SUCCESS );
        assert( DLB_LendCpuMask_sp(handler2, &sp2_process_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows 1 CPU (twice) from the auxiliar mask [0,2,3] (it should prioritize [2,3])
        assert( DLB_BorrowCpusInMask_sp(handler2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 1 && CPU_ISSET(2, &sp2_mask));
        assert( DLB_BorrowCpusInMask_sp(handler2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 1 acquire its CPUs
        assert( DLB_AcquireCpuMask_sp(handler1, &sp1_process_mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask));

        // Both SPs should have their own CPUs
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );
    }

    // Finalize
    assert( DLB_Finalize_sp(handler1) == DLB_SUCCESS );
    assert( DLB_Finalize_sp(handler2) == DLB_SUCCESS );

    return 0;
}
