/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "unique_shmem.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_policies/lewi_mask.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "LB_numThreads/numThreads.h"

#include <assert.h>
#include <string.h>

#define assert_expected(str, cpu_set) \
    do { \
        cpu_set_t expected; \
        mu_parse_mask(str, &expected); \
        assert( CPU_EQUAL(&expected, cpu_set) ); \
    } while(0)


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
    CPU_CLR(cpuid, &sp1_mask);
}

/* Subprocess 2 callbacks */
static void sp2_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp2_mask);
}

static void sp2_cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &sp2_mask);
}

static void wait_for_async_completion(void) {
    if (mode == MODE_ASYNC) {
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        __sync_synchronize();
    }
}

int main(int argc, char *arg[]) {

    // Simulate a SMT architecture
    enum { SYS_NUM_CPUS = 16 };
    enum { SYS_NUM_CORES = 4 };
    enum { SYS_NUM_NODES = 1 };
    mu_testing_set_sys(SYS_NUM_CPUS, SYS_NUM_CORES, SYS_NUM_NODES);
    assert( mu_system_has_smt() );

    // Options
    char options[64] = "--verbose=shmem --shm-key=";
    strcat(options, SHMEM_KEY);

    // Subprocess 1 init
    spd1.id = 111;
    options_init(&spd1.options, options);
    debug_init(&spd1.options);
    mu_parse_mask("0-7", &sp1_mask);
    memcpy(&spd1.process_mask, &sp1_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd1.id, 0, &spd1.process_mask, NULL, spd1.options.shm_key)
            == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd1.id, 0, &spd1.process_mask, spd1.options.shm_key,
                spd1.options.lewi_color) == DLB_SUCCESS );
    assert( pm_callback_set(&spd1.pm, dlb_callback_enable_cpu,
                (dlb_callback_t)sp1_cb_enable_cpu, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd1.pm, dlb_callback_disable_cpu,
                (dlb_callback_t)sp1_cb_disable_cpu, NULL) == DLB_SUCCESS );
    assert( lewi_mask_Init(&spd1) == DLB_SUCCESS );

    // Subprocess 2 init
    spd2.id = 222;
    options_init(&spd2.options, options);
    debug_init(&spd2.options);
    mu_parse_mask("8-15", &sp2_mask);
    memcpy(&spd2.process_mask, &sp2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd2.id, 0, &spd2.process_mask, NULL, spd2.options.shm_key)
            == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd2.id, 0, &spd2.process_mask, spd2.options.shm_key,
                spd2.options.lewi_color) == DLB_SUCCESS );
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

    // Get pointers to the shmem auxiliary cpu sets
    const cpu_set_t *free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
    const cpu_set_t *occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

    /* Lend CPUs in core, they cannot be borrowed until all core is idle */
    {
        // Subprocess 1 lends CPU 3
        CPU_CLR(3, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 3) == DLB_SUCCESS );
        assert_expected("3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Subprocess 2 can't borrow
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_NOUPDT );

        // Subprocess 1 lends CPU 2
        CPU_CLR(2, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 2) == DLB_SUCCESS );
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_NOUPDT );
        assert_expected("2-3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Subprocess 1 lends CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 1) == DLB_SUCCESS );
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_NOUPDT );
        assert_expected("1-3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Subprocess 1 lends CPU 0
        CPU_CLR(0, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 0) == DLB_SUCCESS );
        assert_expected("0-3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Subprocess 2 can now borrow a CPU (the entire core is borrowed)
        assert( lewi_mask_BorrowCpus(&spd2, 1) == DLB_SUCCESS );
        assert_expected("0-3,8-15", &sp2_mask);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert_expected("0-3", occupied_cores);

        // Subprocess 2 lends again and acquires a CPU (the entire core is acquired)
        cpu_set_t cpus_to_lend;
        mu_parse_mask("0-3", &cpus_to_lend);
        mu_substract(&sp2_mask, &sp2_mask, &cpus_to_lend);
        assert( lewi_mask_LendCpuMask(&spd2, &cpus_to_lend) == DLB_SUCCESS );
        assert_expected("8-15", &sp2_mask);
        assert_expected("0-3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );
        assert( lewi_mask_AcquireCpus(&spd2, 1) == DLB_SUCCESS );
        assert_expected("0-3,8-15", &sp2_mask);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert_expected("0-3", occupied_cores);

        // Subprocess 1 reclaims CPU 2
        assert( lewi_mask_ReclaimCpu(&spd1, 2) == DLB_NOTED );
        assert_expected("2,4-7", &sp1_mask);

        if (mode == MODE_POLLING) {
            // Subprocess 2 must return all CPUS in core
            assert( CPU_COUNT(free_cpus) == 0 );
            assert_expected("0-3", occupied_cores);
            assert( lewi_mask_Return(&spd2) == DLB_SUCCESS );
        } else {
            wait_for_async_completion();
        }
        assert( CPU_EQUAL(&sp2_mask, &spd2.process_mask) );
        assert_expected("0-1,3", free_cpus);
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Subprocess 1 can acquire its initial mask
        assert( lewi_mask_Reclaim(&spd1) == DLB_SUCCESS );
        assert_expected("0-7", &sp1_mask);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );
    }

    /* Test asynchronous borrow */
    if (mode == MODE_ASYNC)
    {
        // Subprocess 1 asks for the maximum number of CPUs
        assert( lewi_mask_AcquireCpus(&spd1, DLB_MAX_CPUS) == DLB_NOTED );

        // Subprocess 2 lends core [12-15]
        cpu_set_t cpus_to_lend;
        mu_parse_mask("12-15", &cpus_to_lend);
        mu_substract(&sp2_mask, &sp2_mask, &cpus_to_lend);
        assert( lewi_mask_LendCpuMask(&spd2, &cpus_to_lend) == DLB_SUCCESS );

        wait_for_async_completion();

        // Subprocess 1 should have its mask updated
        assert_expected("0-7,12-15", &sp1_mask);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert_expected("12-15", occupied_cores);

        // Subprocess 2 reclaims all
        assert( lewi_mask_Reclaim(&spd2) == DLB_NOTED );
        assert_expected("8-15", &sp2_mask);

        wait_for_async_completion();

        // Subprocess 1 should have its initial mask
        assert_expected("0-7", &sp1_mask);

        // Remove requests
        assert( lewi_mask_AcquireCpus(&spd1, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
    }

    // Finalize subprocess 1
    assert( lewi_mask_Finalize(&spd1) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd1.id, spd1.options.shm_key, spd1.options.lewi_color)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd1.id, false, spd1.options.shm_key) == DLB_SUCCESS );
    if (mode == MODE_ASYNC) {
        assert( shmem_async_finalize(spd1.id) == DLB_SUCCESS );
    }

    // Finalize subprocess 2
    assert( lewi_mask_Finalize(&spd2) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd2.id, spd2.options.shm_key, spd2.options.lewi_color)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd2.id, false, spd2.options.shm_key) == DLB_SUCCESS );
    if (mode == MODE_ASYNC) {
        assert( shmem_async_finalize(spd2.id) == DLB_SUCCESS );
    }

    return 0;
}
