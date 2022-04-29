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

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_policies/lewi_mask.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static subprocess_descriptor_t spd1;
static subprocess_descriptor_t spd2;
static cpu_set_t sp1_mask;
static cpu_set_t sp2_mask;
static interaction_mode_t mode;

/* Subprocess 1 callbacks */
static void sp1_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp1_mask);
}

static void sp1_cb_disable_cpu(int cpuid, void *arg) {
    if (mode == MODE_ASYNC) {
        lewi_mask_ReturnCpu(&spd1, cpuid);
    }
    CPU_CLR(cpuid, &sp1_mask);
}

/* Subprocess 2 callbacks */
static void sp2_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp2_mask);
}

static void sp2_cb_disable_cpu(int cpuid, void *arg) {
    if (mode == MODE_ASYNC) {
        lewi_mask_ReturnCpu(&spd2, cpuid);
    }
    CPU_CLR(cpuid, &sp2_mask);
}

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Options
    char options[64] = "--verbose=shmem --shm-key=";
    strcat(options, SHMEM_KEY);

    // Initialize constant masks for fast reference
    const cpu_set_t sys_mask = { .__bits = {0xf} };       /* [1111] */
    const cpu_set_t sp1_process_mask = {.__bits={0x3}};   /* [0011] */
    const cpu_set_t sp2_process_mask = {.__bits={0xc}};   /* [1100] */

    // Initialize local masks to [1100] and [0011]
    memcpy(&sp1_mask, &sp1_process_mask, sizeof(cpu_set_t));
    memcpy(&sp2_mask, &sp2_process_mask, sizeof(cpu_set_t));

    // Subprocess 1 init
    spd1.id = 111;
    options_init(&spd1.options, options);
    debug_init(&spd1.options);
    memcpy(&spd1.process_mask, &sp1_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd1.id, 0, &spd1.process_mask, NULL, spd1.options.shm_key)
            == DLB_SUCCESS);
    assert( shmem_cpuinfo__init(spd1.id, 0, &spd1.process_mask, spd1.options.shm_key)
            == DLB_SUCCESS);
    assert( pm_callback_set(&spd1.pm, dlb_callback_enable_cpu,
                (dlb_callback_t)sp1_cb_enable_cpu, NULL) == DLB_SUCCESS);
    assert( pm_callback_set(&spd1.pm, dlb_callback_disable_cpu,
                (dlb_callback_t)sp1_cb_disable_cpu, NULL) == DLB_SUCCESS);
    assert( lewi_mask_Init(&spd1) == DLB_SUCCESS );

    // Subprocess 2 init
    spd2.id = 222;
    options_init(&spd2.options, options);
    memcpy(&spd1.process_mask, &sp1_mask, sizeof(cpu_set_t));
    memcpy(&spd2.process_mask, &sp2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd2.id, 0, &spd2.process_mask, NULL, spd2.options.shm_key)
            == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd2.id, 0, &spd2.process_mask, spd2.options.shm_key)
            == DLB_SUCCESS );
    assert( pm_callback_set(&spd2.pm, dlb_callback_enable_cpu,
                (dlb_callback_t)sp2_cb_enable_cpu, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd2.pm, dlb_callback_disable_cpu,
                (dlb_callback_t)sp2_cb_disable_cpu, NULL) == DLB_SUCCESS );
    assert( lewi_mask_Init(&spd2) == DLB_SUCCESS );

    // Get interaction mode
    assert( spd1.options.mode == spd2.options.mode );
    mode = spd1.options.mode;
    if (mode == MODE_ASYNC) {
        assert( shmem_async_init(spd2.id, &spd2.pm, &spd2.process_mask, spd2.options.shm_key)
                == DLB_SUCCESS );
        assert( shmem_async_init(spd1.id, &spd1.pm, &spd1.process_mask, spd1.options.shm_key)
                == DLB_SUCCESS );
    }

    int err;

    /* Cpu tests */
    {
        // Subprocess 1 wants to acquire CPU 3
        err = lewi_mask_AcquireCpu(&spd1, 3);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );

        // Subprocess 2 no longer needs CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 1 may need to poll again */
        if (mode == MODE_POLLING) {
            assert( lewi_mask_AcquireCpu(&spd1, 3) == DLB_SUCCESS );
        }

        // Poll a certain number of times until mask1 contains CPU 3
        assert_loop( CPU_ISSET(3, &sp1_mask) );

        // Subprocess 2 needs again CPU 3
        assert( lewi_mask_AcquireCpu(&spd2, 3) == DLB_NOTED );

        // Subprocesses 1 and 2 need to poll
        if (mode == MODE_POLLING) {
            assert( lewi_mask_CheckCpuAvailability(&spd2, 3) == DLB_NOTED );
            assert( lewi_mask_ReturnCpu(&spd1, 3) == DLB_SUCCESS );
            assert( lewi_mask_CheckCpuAvailability(&spd2, 3) == DLB_SUCCESS );
        }

        // Poll a certain number of times until mask2 contains CPU 3, and mask1 doesn't
        assert_loop( CPU_ISSET(3, &sp2_mask) );
        assert( !CPU_ISSET(3, &sp1_mask) );

        // Subprocess 1 no longer needs CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 1) == DLB_SUCCESS );

        // Subprocess 2 needs CPU 1
        assert( lewi_mask_AcquireCpu(&spd2, 1) == DLB_SUCCESS );

        // Subprocess 1 reclaims
        assert( lewi_mask_ReclaimCpu(&spd1, 1) == DLB_NOTED );

        // Subprocesses 1 & 2 need to poll
        if (mode == MODE_POLLING) {
            assert( lewi_mask_CheckCpuAvailability(&spd1, 1) == DLB_NOTED );
            assert( lewi_mask_ReturnCpu(&spd2, 1) == DLB_SUCCESS );
            assert( lewi_mask_CheckCpuAvailability(&spd1, 1) == DLB_SUCCESS );
        }
        assert_loop( CPU_ISSET(1, &sp1_mask) );
        assert( !CPU_ISSET(1, &sp2_mask) );

        // Test that SP2 receives CPU 1 again if SP1 lends it again
        if (mode == MODE_ASYNC) {
            // Subprocess 1 lends everything
            CPU_ZERO(&sp1_mask);
            assert( lewi_mask_LendCpuMask(&spd1, &sys_mask) == DLB_SUCCESS );

            // CPU 1 was still requested by Subprocess 2
            assert_loop( CPU_ISSET(1, &sp2_mask) );

            // Subprocess 2 lends CPU 1
            CPU_CLR(1, &sp2_mask);
            assert( lewi_mask_LendCpu(&spd2, 1) == DLB_SUCCESS );

            // Subprocess aquires its mask
            assert( lewi_mask_AcquireCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );
            assert_loop( CPU_ISSET(0, &sp1_mask) );
            assert_loop( CPU_ISSET(1, &sp1_mask) );
        }
    }

    /* CpuMask tests */
    {
        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( lewi_mask_LendCpuMask(&spd1, &sys_mask) == DLB_SUCCESS );

        // Subprocess 2 acquires everything
        assert( lewi_mask_AcquireCpuMask(&spd2, &sys_mask) == DLB_SUCCESS );

        // Subprocess 1 reclaims its CPUs
        assert( lewi_mask_ReclaimCpuMask(&spd1, &sp1_process_mask) == DLB_NOTED );

        // Subprocess 1 reclaims everything
        assert( lewi_mask_ReclaimCpuMask(&spd1, &sys_mask) == DLB_ERR_PERM );

        // Subprocess 2 lends external CPUs
        CPU_CLR(0, &sp2_mask);
        CPU_CLR(1, &sp2_mask);
        assert( lewi_mask_LendCpuMask(&spd2, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocesses 1 needs to poll to set guest field
        if (mode == MODE_POLLING) {
            assert_loop( lewi_mask_CheckCpuAvailability(&spd1, 0) == DLB_SUCCESS );
            assert_loop( lewi_mask_CheckCpuAvailability(&spd1, 1) == DLB_SUCCESS );
        }

        // Check everyone has their CPUs
        assert_loop( CPU_EQUAL(&sp1_process_mask, &sp1_mask) );
        assert_loop( CPU_EQUAL(&sp2_process_mask, &sp2_mask) );
    }

    /* Cpus tests */
    {
        // Subprocess 1 lends CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 1) == DLB_SUCCESS );

        // Subprocess 2 acquires one CPU
        assert( lewi_mask_AcquireCpus(&spd2, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(1, &sp2_mask) );

        // Subprocess 2 acquires one CPU again
        err = lewi_mask_AcquireCpus(&spd2, 1);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );

        // Subprocess 1 acquires 1 CPU, it should reclaim CPU 1
        assert( lewi_mask_AcquireCpus(&spd1, 1) == DLB_NOTED );
        assert_loop( CPU_ISSET(1, &sp1_mask) );

        // Subprocess 2 returns CPU 1 (async returns by callback)
        if (mode == MODE_POLLING) {
            assert( CPU_ISSET(1, &sp2_mask) );
            CPU_CLR(1, &sp2_mask);
            assert( lewi_mask_ReturnCpu(&spd2, 1) == DLB_SUCCESS );
        }

        // Subprocess 1 needs to poll to set guest field
        if (mode == MODE_POLLING) {
            assert_loop( lewi_mask_CheckCpuAvailability(&spd1, 1) == DLB_SUCCESS );
        }

        // Subprocess 2 no longer wants external CPUs
        assert( lewi_mask_AcquireCpus(&spd2, 0) == DLB_SUCCESS );   /* general queue */
        assert( lewi_mask_LendCpu(&spd2, 1) == DLB_SUCCESS );       /* CPU queue */
    }

    /* Reset test */
    {
        // Subprocess 2 lends CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 1 acquires one CPU
        assert( lewi_mask_AcquireCpus(&spd1, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp1_mask) && CPU_COUNT(&sp1_mask) == 3 );

        // Subprocess 1 decides to shut down CPU 1 without notifying DLB
        CPU_CLR(1, &sp1_mask);

        // Subprocess 1 resets. CPU 1 should not be re-enabled
        assert( lewi_mask_DisableDLB(&spd1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(0, &sp1_mask) && CPU_COUNT(&sp1_mask) == 1 );

        // Subprocess 2 recovers CPU 3
        assert( lewi_mask_AcquireCpu(&spd2, 3) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp2_mask) );

        // Subprocess 1 enables DLB and CPU 1
        assert( lewi_mask_EnableDLB(&spd1) == DLB_SUCCESS );
        CPU_SET(1, &sp1_mask);
    }

    /* Reset test with requested CPUs */
    if (mode == MODE_ASYNC)
    {
        // Subprocess 2 lends CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 1 acquires one CPU
        assert( lewi_mask_AcquireCpus(&spd1, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp1_mask) && CPU_COUNT(&sp1_mask) == 3 );

        // Subprocess 2 requests 1 CPU
        assert( lewi_mask_AcquireCpus(&spd2, 1) == DLB_NOTED );

        // Subprocess 1 resets, CPU 3 should go to Subprocess 2
        assert( lewi_mask_DisableDLB(&spd1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(3, &sp2_mask) && CPU_COUNT(&sp2_mask) == 2 );

        // Subprocess 1 enables DLB
        assert( lewi_mask_EnableDLB(&spd1) == DLB_SUCCESS );
    }

    /* Last borrow optimization test */
    {
        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( lewi_mask_LendCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows 1 CPU 3 times
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_SUCCESS );
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_SUCCESS );
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_NOUPDT );
        assert_loop( CPU_COUNT(&sp2_mask) == 4
                && CPU_ISSET(0, &sp2_mask) && CPU_ISSET(1, &sp2_mask) );

        // Subprocess 2 returns borrowed CPUs
        CPU_CLR(0, &sp2_mask);
        CPU_CLR(1, &sp2_mask);
        assert( lewi_mask_LendCpuMask(&spd2, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 1 acquire its CPUs
        assert( lewi_mask_AcquireCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
    }

    /* MaxParallelism */
    {
        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( lewi_mask_LendCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows everything
        assert( lewi_mask_Borrow(&spd2) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 4
                && CPU_ISSET(0, &sp2_mask) && CPU_ISSET(1, &sp2_mask) );

        // Subprocess 1 acquire its CPUs
        assert( lewi_mask_AcquireCpuMask(&spd1, &sp1_process_mask) == DLB_NOTED );

        // Subprocess 2 sets max_parallelism to 2
        lewi_mask_SetMaxParallelism(&spd2, 2);

        // Both SPs should have their own CPUs
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                 && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                 && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );

        // Subprocess 1 lends everything
        CPU_ZERO(&sp1_mask);
        assert( lewi_mask_LendCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows everything (can't, max_parallelism still 2)
        assert( lewi_mask_Borrow(&spd2) == DLB_NOUPDT );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                 && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );

        // Subprocess 1 acquire its CPUs
        assert( lewi_mask_AcquireCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 2 resets max_parallelism
        lewi_mask_UnsetMaxParallelism(&spd2);
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
        assert( lewi_mask_LendCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );

        // Subprocess 2 lends 1 CPU
        CPU_CLR(2, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 2) == DLB_SUCCESS );

        // Subprocess 2 tries to acquire 1 CPU from the auxiliar mask [0,2,3] (it should prioritize [2,3])
        assert( lewi_mask_AcquireCpusInMask(&spd2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 2 lends 1 CPU, again
        CPU_CLR(2, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 2) == DLB_SUCCESS );

        // Subprocess 2 tries to acquire 4 CPUs from the auxiliar mask [0,2,3]
        err = lewi_mask_AcquireCpusInMask(&spd2, 4, &mask);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 3
                && CPU_ISSET(0, &sp2_mask) && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 2 removes any previous requests
        if (mode == MODE_ASYNC) {
            assert( lewi_mask_AcquireCpus(&spd2, 0) == DLB_SUCCESS );
        }

        // Subprocess 2 lends everything (0 first, and 2-3 later)
        CPU_ZERO(&sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 0) == DLB_SUCCESS );
        assert( lewi_mask_LendCpuMask(&spd2, &sp2_process_mask) == DLB_SUCCESS );

        // Subprocess 2 borrows 1 CPU (twice) from the auxiliar mask [0,2,3] (it should prioritize [2,3])
        assert( lewi_mask_BorrowCpusInMask(&spd2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 1 && CPU_ISSET(2, &sp2_mask));
        assert( lewi_mask_BorrowCpusInMask(&spd2, 1, &mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask));

        // Subprocess 1 acquire its CPUs
        assert( lewi_mask_AcquireCpuMask(&spd1, &sp1_process_mask) == DLB_SUCCESS );
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask));

        // Both SPs should have their own CPUs
        assert_loop( CPU_COUNT(&sp1_mask) == 2
                && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );
    }

    /* Into / Out of Blocking Calls */
    /* FIXME: this test has a race condition in mode=async */
    if (mode != MODE_ASYNC)
    {
        // Enable --lewi-keep-one-cpu
        spd1.options.lewi_keep_cpu_on_blocking_call = true;
        assert( lewi_mask_IntoBlockingCall(&spd1) == DLB_NOUPDT );
        assert( lewi_mask_OutOfBlockingCall(&spd1, 0) == DLB_NOUPDT );

        // These functions get the CPU id from sched_getcpu()
        // Force binding to CPU 0 or skip test
        const cpu_set_t mask = { .__bits = {0x1} };       /* [0001] */
        sched_setaffinity(0, sizeof(cpu_set_t), &mask);
        if (sched_getcpu() == 0) {

            // Disable --lewi-keep-one-cpu
            spd1.options.lewi_keep_cpu_on_blocking_call = false;
            spd2.options.lewi_keep_cpu_on_blocking_call = false;

            /* No conflict */
            {
                // Subprocess 1 enters a blocking call
                assert( lewi_mask_IntoBlockingCall(&spd1) == DLB_SUCCESS );

                // Subprocess 2 may borrow CPU 0
                assert( lewi_mask_BorrowCpu(&spd2, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp2_mask) == 3 && CPU_ISSET(0, &sp2_mask) );

                // Subprocess 2 lends CPU 0
                CPU_CLR(0, &sp2_mask);
                assert( lewi_mask_LendCpu(&spd2, 0) == DLB_SUCCESS );

                // Subprocess 1 leaves the blocking call
                assert( lewi_mask_OutOfBlockingCall(&spd1, 0) == DLB_SUCCESS );
            }

            /* Subprocess 1 reclaims */
            {
                // Subprocess 1 enters a blocking call
                assert( lewi_mask_IntoBlockingCall(&spd1) == DLB_SUCCESS );

                // Subprocess 2 may borrow CPU 0
                assert( lewi_mask_BorrowCpu(&spd2, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp2_mask) == 3 && CPU_ISSET(0, &sp2_mask) );

                // Subprocess 1 leaves the blocking call, CPU 0 is reclaimed
                assert( lewi_mask_OutOfBlockingCall(&spd1, 0) == DLB_NOTED );

                // Subprocesses 2 needs to poll
                if (mode == MODE_POLLING) {
                    assert( lewi_mask_CheckCpuAvailability(&spd1, 0) == DLB_NOTED );
                    assert( lewi_mask_ReturnCpu(&spd2, 0) == DLB_SUCCESS );
                    assert( lewi_mask_CheckCpuAvailability(&spd1, 0) == DLB_SUCCESS );
                }

                // Both SPs should have their own CPUs
                assert_loop( CPU_COUNT(&sp1_mask) == 2
                        && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
                assert_loop( CPU_COUNT(&sp2_mask) == 2
                        && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );

                // Subprocesses 2 removes the request over CPU 0
                if (mode == MODE_ASYNC) {
                    assert( lewi_mask_LendCpu(&spd2, 0) == DLB_SUCCESS );
                }
            }

            /* MPI with a non-owned CPU, no conflict */
            {
                // Subprocess 1 lends CPU 0
                CPU_CLR(0, &sp1_mask);
                assert( lewi_mask_LendCpu(&spd1, 0) == DLB_SUCCESS );

                // Subprocess 2 borrows CPU 0
                assert( lewi_mask_BorrowCpu(&spd2, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp2_mask) == 3 && CPU_ISSET(0, &sp2_mask) );

                // Subprocess 2 enters a blocking call (with CPU 0)
                assert( lewi_mask_IntoBlockingCall(&spd2) == DLB_SUCCESS );

                // Subprocess 1 may borrow CPU 0
                assert( lewi_mask_BorrowCpu(&spd1, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp1_mask) == 2 && CPU_ISSET(0, &sp1_mask) );

                // Subprocess 1 lends CPU 0
                CPU_CLR(0, &sp1_mask);
                assert( lewi_mask_LendCpu(&spd1, 0) == DLB_SUCCESS );

                // Subprocess 2 leaves the blocking call
                assert( lewi_mask_OutOfBlockingCall(&spd2, 0) == DLB_SUCCESS );

                // Subprocess 2 lends CPU 0
                CPU_CLR(0, &sp2_mask);
                assert( lewi_mask_LendCpu(&spd2, 0) == DLB_SUCCESS );

                // Subprocess 1 acquires CPU 0
                assert( lewi_mask_AcquireCpu(&spd1, 0) == DLB_SUCCESS );

                // Both SPs should have their own CPUs
                assert_loop( CPU_COUNT(&sp1_mask) == 2
                        && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
                assert_loop( CPU_COUNT(&sp2_mask) == 2
                        && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );
            }

            /* MPI with a non-owned CPU, Subprocess 1 reclaims */
            {
                // Subprocess 1 lends CPU 0
                CPU_CLR(0, &sp1_mask);
                assert( lewi_mask_LendCpu(&spd1, 0) == DLB_SUCCESS );

                // Subprocess 2 borrows CPU 0
                assert( lewi_mask_BorrowCpu(&spd2, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp2_mask) == 3 && CPU_ISSET(0, &sp2_mask) );

                // Subprocess 2 enters a blocking call (with CPU 0)
                assert( lewi_mask_IntoBlockingCall(&spd2) == DLB_SUCCESS );

                // Subprocess 1 acquires CPU 0
                assert( lewi_mask_AcquireCpu(&spd1, 0) == DLB_SUCCESS );
                assert_loop( CPU_COUNT(&sp1_mask) == 2 && CPU_ISSET(0, &sp1_mask) );

                // Subprocess 2 leaves the blocking call
                assert( lewi_mask_OutOfBlockingCall(&spd2, 0) == DLB_NOUPDT );

                // Subprocesses 2 needs to poll
                if (mode == MODE_POLLING) {
                    assert_loop( CPU_COUNT(&sp2_mask) == 3 && CPU_ISSET(0, &sp2_mask) );
                    assert( lewi_mask_ReturnCpu(&spd2, 0) == DLB_SUCCESS );
                    assert_loop( CPU_COUNT(&sp2_mask) == 2 && !CPU_ISSET(0, &sp2_mask) );
                }

                // Both SPs should have their own CPUs
                assert_loop( CPU_COUNT(&sp1_mask) == 2
                        && CPU_ISSET(0, &sp1_mask) && CPU_ISSET(1, &sp1_mask) );
                assert_loop( CPU_COUNT(&sp2_mask) == 2
                        && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );

                // Subprocesses 2 removes the request over CPU 0
                if (mode == MODE_ASYNC) {
                    assert( lewi_mask_LendCpu(&spd2, 0) == DLB_SUCCESS );
                }
            }
        } else {
            printf("Skipping Into / Out of Blocking Calls tests\n");
        }
    }

    // Finalize subprocess 1
    assert( lewi_mask_Finalize(&spd1) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd1.id, spd1.options.shm_key) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd1.id, false, spd1.options.shm_key) == DLB_SUCCESS );
    if (mode == MODE_ASYNC) {
        assert( shmem_async_finalize(spd1.id) == DLB_SUCCESS );
    }

    /* Tests with unregistered CPUs */
    {
        // Subprocess 2 lends CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 2 acquires everything (Mask operations does not return DLB_ERR_PERM anymore)
        assert( lewi_mask_AcquireCpuMask(&spd2, &sys_mask ) == DLB_SUCCESS );

        // Subprocess 2 acquires CPU 0 (CPU operations must return DLB_ERR_PERM)
        assert( lewi_mask_AcquireCpu(&spd2, 0 ) == DLB_ERR_PERM );

        // Subprocess 2 should have its own CPUs
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                 && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );

        // Subprocess 2 lends CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 2 reclaims everything
        assert( lewi_mask_ReclaimCpuMask(&spd2, &sys_mask ) == DLB_ERR_PERM );

        // Subprocess 2 should have its own CPUs
        assert_loop( CPU_COUNT(&sp2_mask) == 2
                 && CPU_ISSET(2, &sp2_mask) && CPU_ISSET(3, &sp2_mask) );
    }

    // Finalize subprocess 2
    assert( lewi_mask_Finalize(&spd2) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd2.id, spd2.options.shm_key) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd2.id, false, spd2.options.shm_key) == DLB_SUCCESS );
    if (mode == MODE_ASYNC) {
        assert( shmem_async_finalize(spd2.id) == DLB_SUCCESS );
    }

    return 0;
}
