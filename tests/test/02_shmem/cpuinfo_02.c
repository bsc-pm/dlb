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

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

// Basic checks with 2 sub-processes with masks

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

    // Construct some CPU priority arrays
    array_cpuid_t cpus_0_1;
    array_cpuid_t_init(&cpus_0_1, SYS_SIZE);
    array_cpuid_t_push(&cpus_0_1, 0);
    array_cpuid_t_push(&cpus_0_1, 1);

    array_cpuid_t cpus_2_3;
    array_cpuid_t_init(&cpus_2_3, SYS_SIZE);
    array_cpuid_t_push(&cpus_2_3, 2);
    array_cpuid_t_push(&cpus_2_3, 3);

    array_cpuid_t cpus_2_3_0;
    array_cpuid_t_init(&cpus_2_3_0, SYS_SIZE);
    array_cpuid_t_push(&cpus_2_3_0, 2);
    array_cpuid_t_push(&cpus_2_3_0, 3);
    array_cpuid_t_push(&cpus_2_3_0, 0);

    array_cpuid_t cpus_2_3_0_1;
    array_cpuid_t_init(&cpus_2_3_0_1, SYS_SIZE);
    array_cpuid_t_push(&cpus_2_3_0_1, 2);
    array_cpuid_t_push(&cpus_2_3_0_1, 3);
    array_cpuid_t_push(&cpus_2_3_0_1, 0);
    array_cpuid_t_push(&cpus_2_3_0_1, 1);

    int64_t last_borrow = 0;
    int requested_ncpus;

    // Init
    assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

    // Get pointers to the shmem auxiliary cpu sets
    const cpu_set_t *free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
    const cpu_set_t *occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

    // Initialize options and enable queues if needed
    options_t options;
    options_init(&options, NULL);
    bool async = options.mode == MODE_ASYNC;
    bool polling = options.mode == MODE_POLLING;
    if (async) {
        shmem_cpuinfo__enable_request_queues();
    }

    int err;

    /*** Successful ping-pong ***/
    {
        // Process 1 wants CPUs 2 & 3
        requested_ncpus = 2;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            assert( tasks.count == 0 );
        }

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                        &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                    == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // Process 1 cannot reclaim CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_ERR_PERM );
        assert( tasks.count == 0 );

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_NOTED );
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

        // Process 1 returns CPUs 2 & 3
        if (polling) {
            assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == DISABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu_mask(p1_pid, &p2_mask);
        }

        // Process 2 releases CPUs 2 & 3 again
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( async ? tasks.count == 2
                && tasks.items[0].pid == p1_pid && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU
                && tasks.items[1].pid == p1_pid && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU
                : tasks.count == 0 );
        array_cpuinfo_task_t_clear(&tasks);

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                        &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                    == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // Process 2 reclaims CPUs 2 & 3 again
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_NOTED );
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

        // Process 1 returns CPUs 2 & 3 and removes petition
        if (polling) {
            assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == DISABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu_mask(p1_pid, &p2_mask);
        }
        assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 2 releases CPUs 2 & 3, checks no victim and reclaims
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );
    }

    /*** Successful ping-pong, but using reclaim_all and return_all ***/
    {
        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 1 wants CPUs 2 & 3
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                    &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

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

        // Process 1 returns all
        if (polling) {
            assert( shmem_cpuinfo__return_all(p1_pid, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == DISABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu_mask(p1_pid, &p2_mask);
        }

        // Process 2 releases CPUs 2 & 3 again
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            assert( tasks.count == 0 );
        }

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                        &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                    == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        }

        // Process 2 reclaims all again
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

        // Process 1 returns all and removes petition
        if (polling) {
            assert( shmem_cpuinfo__return_all(p1_pid, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 2 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == DISABLE_CPU );
            assert( tasks.items[1].pid == p1_pid
                    && tasks.items[1].cpuid == 3
                    && tasks.items[1].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu_mask(p1_pid, &p2_mask);
            assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 0 );
        }

        // Process 2 releases CPUs 2 & 3, checks no new guests and reclaims
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** Late reply ***/
    {
        // Process 1 wants CPUs 2 & 3
        requested_ncpus = 2;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );

        // Process 1 no longer wants CPUs 2 & 3
        shmem_cpuinfo__remove_requests(p1_pid);

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** MaxParallelism ***/
    {
        // Process 1 lends CPU 0
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // TEST 1: process 2 is guesting 0,2-3 and sets max_parellelism to 1
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // TEST 2: process 2 is guesting 0,2-3 and sets max_parellelism to 2
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // TEST 3: process 2 is guesting 0,2-3 and sets max_parellelism to 3
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Process 1 lends CPU 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // TEST 4: process 2 is guesting 0-3 and sets max_parellelism to 1
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 3 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == DISABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 1
                && tasks.items[2].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // TEST 5: process 2 is guesting 0-3 and sets max_parellelism to 2
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 3 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == ENABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 1
                && tasks.items[2].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 1
                && tasks.items[1].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // TEST 6: process 2 is guesting 0-3 and sets max_parellelism to 3, P1 reclaims
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 1
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__reclaim_all(p1_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 4 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == ENABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 1
                && tasks.items[2].action == DISABLE_CPU );
        assert( tasks.items[3].pid == p1_pid
                && tasks.items[3].cpuid == 1
                && tasks.items[3].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 1
                && tasks.items[1].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // Process 2 lends CPU 0
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    /*** Test CPU ownership changes ***/
    {
        cpu_set_t mask;

        // Remove CPU 3 ownership from Process 2
        mu_parse_mask("2", &mask);
        shmem_cpuinfo__update_ownership(p2_pid, &mask, &tasks);
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 3) == DLB_ERR_PERM );
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_ERR_PERM );
        // Recover
        mu_parse_mask("2-3", &mask);
        shmem_cpuinfo__update_ownership(p2_pid, &mask, &tasks);
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 3) == DLB_ERR_PERM );
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );

        // Remove CPU 3 ownership from Process 2 while in LENT state
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        mu_parse_mask("2", &mask);
        shmem_cpuinfo__update_ownership(p2_pid, &mask, &tasks);
        assert( tasks.count == 0 );
        if (polling) {
            assert( shmem_cpuinfo__return_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu(p1_pid, 3);
        }
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_ERR_PERM );
        // Recover
        mu_parse_mask("2-3", &mask);
        shmem_cpuinfo__update_ownership(p2_pid, &mask, &tasks);
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 3) == DLB_ERR_PERM );
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );

        // Steal ownership of CPU 3 from Process 2 to Process 1, while CPU 0 is LENT
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        mu_parse_mask("0-1,3", &mask);
        shmem_cpuinfo__update_ownership(p1_pid, &mask, &tasks);
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        if (polling) {
            assert( shmem_cpuinfo__return_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == DISABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
        } else {
            shmem_cpuinfo__return_async_cpu(p2_pid, 3);
        }
        // CPU 0 is still guested by Process2
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 0) == DLB_SUCCESS );
        // CPU 3 is available to Process1
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 3) == DLB_SUCCESS );
        // Recover
        mu_parse_mask("0-1", &mask);
        shmem_cpuinfo__update_ownership(p1_pid, &mask, &tasks);
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 3) == DLB_ERR_PERM );
        mu_parse_mask("2-3", &mask);
        shmem_cpuinfo__update_ownership(p2_pid, &mask, &tasks);
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
    }

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

    /* Test lend post mortem feature */
    {
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
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                    &cpus_2_3_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
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
        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P2 lends CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // P1 acquires CPU 2 & 3
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, NULL /* requested_ncpus */,
                    &cpus_2_3, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        // P2 reclaims its CPU and requests the rest
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_NOTED );
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
        requested_ncpus = 2;
        err = shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                &cpus_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );

        // P1 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        if (async) {
            // P2 gets CPUs 0-3
            assert( tasks.count == 4 );
            for (int i = 0; i < 4; ++i) {
                assert( tasks.items[i].pid == p2_pid
                        && tasks.items[i].cpuid == i
                        && tasks.items[i].action == ENABLE_CPU );
            }
        } else {
            // P2 gets CPU 2-3
            assert( tasks.count == 2 );
            for (int i = 0; i < 2; ++i) {
                assert( tasks.items[i].pid == p2_pid
                        && tasks.items[i].cpuid == i+2
                        && tasks.items[i].action == ENABLE_CPU );
            }
        }
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

        // If polling, P2 needs to ask again for CPUs 0-3
        if (polling) {
            assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 2) == DLB_SUCCESS );
            assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );
            assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, NULL /* requested_ncpus */,
                        &cpus_2_3_0_1, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */, &last_borrow,
                        &tasks) == DLB_SUCCESS );
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
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    mu_finalize();

    return 0;
}
