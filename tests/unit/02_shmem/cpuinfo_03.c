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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "support/types.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

// Basic checks for checking Borrow and Acquire features

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Initialize local masks to [1100] and [0011]
    pid_t p1_pid = 111;
    cpu_set_t p1_mask;
    CPU_ZERO(&p1_mask);
    CPU_SET(0, &p1_mask);
    CPU_SET(1, &p1_mask);
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    CPU_ZERO(&p2_mask);
    CPU_SET(2, &p2_mask);
    CPU_SET(3, &p2_mask);
    array_cpuinfo_task_t tasks;
    array_cpuinfo_task_t_init(&tasks, SYS_SIZE);

    // Init
    assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

    // Initialize options and enable queues if needed
    options_t options;
    options_init(&options, NULL);
    bool async = options.mode == MODE_ASYNC;
    if (async) {
        shmem_cpuinfo__enable_request_queues();
    }
    int64_t last_borrow_value = 0;
    int64_t *last_borrow = async ? NULL : &last_borrow_value;
    int requested_ncpus;

    // Setup dummy priority CPUs
    array_cpuid_t cpus_priority_array;
    array_cpuid_t_init(&cpus_priority_array, SYS_SIZE);
    for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

    int err;

    /*** BorrowCpus test ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 1 wants to BORROW up to 2 CPUs
        requested_ncpus = 2;
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 2 releases CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 3 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == DISABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 3
                && tasks.items[2].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 && tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3 && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** AcquireCpus test ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 1 wants to ACQUIRE 2 CPUs
        requested_ncpus = 2;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 2 releases CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( async ? tasks.count == 1 && tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2 && tasks.items[0].action == ENABLE_CPU
                : tasks.count == 0 );
        array_cpuinfo_task_t_clear(&tasks);

        // If polling, process 1 needs to ask again for the last CPU
        if (!async) {
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 4 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 2
                && tasks.items[1].action == ENABLE_CPU );
        assert( tasks.items[2].pid == p1_pid
                && tasks.items[2].cpuid == 3
                && tasks.items[2].action == DISABLE_CPU );
        assert( tasks.items[3].pid == p2_pid
                && tasks.items[3].cpuid == 3
                && tasks.items[3].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 releases CPU 2 and 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 && tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2 && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 && tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3 && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** AcquireCpus with late reply ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 1 wants to ACQUIRE 2 CPUs
        requested_ncpus = 2;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 wants to cancel previous request
        shmem_cpuinfo__remove_requests(p1_pid);

        // Process 1 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** AcquireCpus one by one with some CPUs already reclaimed ***/
    {
        // Process 1 releases CPUs 0 and 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 2 acquires CPU 0
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 acquires 1 CPU (acquires CPU 1 first, currently idle)
        requested_ncpus = 1;
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 acquires 1 CPU (reclaims CPU 0)
        requested_ncpus = 1;
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 2 returns CPU 0
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 && tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 0 && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** AcquireCpus of CPUs not in the priority array ***/
    {
        // Process 2 acquires 1 CPU, but only allowing CPUs [2, 3, 1]
        array_cpuid_t_clear(&cpus_priority_array);
        array_cpuid_t_push(&cpus_priority_array, 2);
        array_cpuid_t_push(&cpus_priority_array, 3);
        array_cpuid_t_push(&cpus_priority_array, 1);
        requested_ncpus = 1;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );

        // Process 1 releases CPU 0
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // If polling, process 2 needs to ask again (no update because CPU 0 is not eligible)
        if (!async) {
            requested_ncpus = 1;
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                        &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                        last_borrow, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
        }

        // Process 1 acquires CPU 0 (currently idle)
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 1 releases CPU 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &tasks) == DLB_SUCCESS );
        assert( async ? tasks.count == 1 && tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 1 && tasks.items[0].action == ENABLE_CPU
                : tasks.count == 0 );
        array_cpuinfo_task_t_clear(&tasks);

        // If polling, process 2 needs to ask again
        if (!async) {
            requested_ncpus = 1;
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                        &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                        last_borrow, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 1
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // Process 2 releases CPU 1
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // 1 acquires CPU 1 (currently idle)
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

    /* Test lend post mortem feature */
    {
        // Reset cpu priority
        array_cpuid_t_clear(&cpus_priority_array);
        for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P1 finalizes
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

        // P2 borrows P1 CPUs
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 1
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P2 finalizes
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    /* Test early finalization with pending actions */
    {
        // Reset cpu priority
        array_cpuid_t_clear(&cpus_priority_array);
        for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P2 asks for as many CPUs as SYS_SIZE
        requested_ncpus = SYS_SIZE;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );

        // P1 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 0
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p2_pid
                    && tasks.items[1].cpuid == 1
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            assert( tasks.count == 0 );
        }
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

        // If polling, P2 needs to ask again for SYS_SIZE CPUs
        if (!async) {
            requested_ncpus = SYS_SIZE;
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                        &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                        last_borrow, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 0
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p2_pid
                    && tasks.items[1].cpuid == 1
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // P2 finalizes
        assert( shmem_cpuinfo__deregister(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    /* Test asynchronous acquire ncpus with disabled CPUs */
    if (async) {

        // Reset cpu priority
        array_cpuid_t_clear(&cpus_priority_array);
        for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

        // Set up fake spd with default options
        subprocess_descriptor_t spd = {};
        spd.options.lewi_respect_cpuset = true;
        spd.options.debug_opts = DBG_CLEAR;
        spd_enter_dlb(&spd);

        // Initialize P1 with an empty mask
        cpu_set_t p1_empty_mask;
        CPU_ZERO(&p1_empty_mask);
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_empty_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        shmem_cpuinfo__enable_request_queues();

        // P1 asks for all CPUs
        requested_ncpus = DLB_MAX_CPUS;
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_NOTED );
        assert( tasks.count == 0 );
        assert( requested_ncpus == DLB_MAX_CPUS );

        // P2 initializes
        cpu_set_t p2_full_mask;
        mu_get_system_mask(&p2_full_mask);
        assert( shmem_cpuinfo__init(p2_pid, 0, &p2_full_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

        // P2 lends CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P2 reclaims CPU 3
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P1 returns
        shmem_cpuinfo__return_async_cpu(p1_pid, 3);

        // P2 lends again CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Finalize
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__deregister(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    /* Test asynchronous acquire mask with disabled CPUs */
    if (async) {

        // Reset cpu priority
        array_cpuid_t_clear(&cpus_priority_array);
        for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

        // Set up fake spd with default options
        subprocess_descriptor_t spd = {};
        spd.options.lewi_respect_cpuset = true;
        spd.options.debug_opts = DBG_CLEAR;
        spd_enter_dlb(&spd);

        // Initialize P1 with an empty mask
        cpu_set_t p1_empty_mask;
        CPU_ZERO(&p1_empty_mask);
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_empty_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        shmem_cpuinfo__enable_request_queues();

        // P1 asks for all CPUs (passing NULL as requested_ncpus and having all CPUs in
        // cpus_priority_array is equivalent for asking for a full mask)
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    last_borrow, &tasks) == DLB_NOTED );
        assert( tasks.count == 0 );
        assert( requested_ncpus == DLB_MAX_CPUS );

        // P2 initializes
        cpu_set_t p2_full_mask;
        mu_get_system_mask(&p2_full_mask);
        assert( shmem_cpuinfo__init(p2_pid, 0, &p2_full_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

        // P2 lends CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P2 reclaims CPU 3
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P1 returns
        shmem_cpuinfo__return_async_cpu(p1_pid, 3);

        // P2 lends again CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Finalize
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__deregister(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    mu_finalize();

    return 0;
}
