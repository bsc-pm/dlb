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

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

// Basic checks with 2 sub-processes

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Initialize local masks to [1100] and [0011]
    pid_t p1_pid = 111;
    cpu_set_t p1_mask;
    mu_parse_mask("0-1", &p1_mask);
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    mu_parse_mask("2-3", &p2_mask);
    array_cpuinfo_task_t tasks;
    array_cpuinfo_task_t_init(&tasks, SYS_SIZE);

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
        // Process 1 wants CPU 3
        err = shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );
        } else {
            assert( tasks.count == 0 );
            assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(3, free_cpus) );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }

        // If polling, process 1 needs to ask again for CPU 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );
        }

        // Process 1 cannot reclaim CPU 3
        assert( shmem_cpuinfo__reclaim_cpu(p1_pid, 3, &tasks) == DLB_ERR_PERM );
        assert( tasks.count == 0 );

        // Process 2 reclaims CPU 3
        assert( shmem_cpuinfo__reclaim_cpu(p2_pid, 3, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );

        // Process 1 returns CPU 3
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
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 releases CPU 3 again
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );
        } else {
            assert( tasks.count == 0 );
            assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(3, free_cpus) );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }

        // If polling, process 1 needs to ask again for CPU 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p1_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );
        }

        // Process 2 reclaims CPU 3 again
        assert( shmem_cpuinfo__reclaim_cpu(p2_pid, 3, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(3, occupied_cores) );

        // Process 1 returns CPU 3
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
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process removes petition of CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 releases CPU 3, checks no victim and reclaims
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );
    }


    /*** Late reply ***/
    {
        // Process 1 wants CPU 3
        err = shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 1 no longer wants CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 reclaims CPU 3
        assert( shmem_cpuinfo__reclaim_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );
    }

    /*** MaxParallelism ***/
    {
        // Process 1 lends CPU 0
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(0, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 acquires CPU 0
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(0, occupied_cores) );

        // Process 2 sets max_parallelism to 1 (CPUs 0 and 3 should be removed)
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 0
                && tasks.items[1].action == DISABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 2
                && CPU_ISSET(0, free_cpus)
                && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 acquires CPU 3
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(0, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // Process 2 sets max_parallelism to 2 (no CPU should be removed)
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(0, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );
    }

    /*** Errors ***/
    assert( shmem_cpuinfo__return_cpu(p1_pid, 3, &tasks) == DLB_ERR_PERM );
    assert( tasks.count == 0 );

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_ERR_NOSHMEM );

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

        // Update pointers of the new shmem
        free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
        occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

        // P1 finalizes
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( CPU_EQUAL(free_cpus, &p1_mask) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P2 acquires P1 CPUs
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 1, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P2 may poll for reclaimed resources, no action for public CPUs
        if (polling) {
            assert( shmem_cpuinfo__return_all(p2_pid, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
            assert( shmem_cpuinfo__return_cpu_mask(p2_pid, &p1_mask, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
            assert( shmem_cpuinfo__return_cpu(p2_pid, 0, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
        }

        // P2 finalizes
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    /* Test respect-cpuset=no feature */
    {
        // Set up fake spd to set respect-cpuset option
        subprocess_descriptor_t spd;
        spd.options.lewi_respect_cpuset = false;
        spd_enter_dlb(&spd);

        // Initialize
        cpu_set_t reduced_p1_mask;
        mu_parse_mask("0", &reduced_p1_mask);
        assert( shmem_cpuinfo__init(p1_pid, 0, &reduced_p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        cpu_set_t reduced_p2_mask;
        mu_parse_mask("2", &reduced_p2_mask);
        assert( shmem_cpuinfo__init(p2_pid, 0, &reduced_p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // Update pointers of the new shmem
        free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
        occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

        // Not yet registered CPUs count as free CPUs
        assert( CPU_COUNT(free_cpus) == 2
                && CPU_ISSET(1, free_cpus)
                && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 borrows CPU 3
        assert( shmem_cpuinfo__borrow_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(1, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 may poll for reclaimed resources, no action for public CPUs
        if (polling) {
            assert( shmem_cpuinfo__return_all(p1_pid, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
            assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
            assert( shmem_cpuinfo__return_cpu(p1_pid, 3, &tasks) == DLB_NOUPDT );
            assert( tasks.count == 0 );
        }

        // P1 lends CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 2
                && CPU_ISSET(1, free_cpus)
                && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 borrows CPU 3 from cpuset
        int requested_ncpus = 1;
        array_cpuid_t cpus_priority_array;
        array_cpuid_t_init(&cpus_priority_array, SYS_SIZE);
        array_cpuid_t_push(&cpus_priority_array, 3);
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO,
                    /* max_parallelism */ 0, /* last_borrow */ NULL, &tasks)
                == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(1, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 lends CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 2
                && CPU_ISSET(1, free_cpus)
                && CPU_ISSET(3, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 aquires CPU 3
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 3
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(1, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P2 wants to aquire full mask, acquires [1], [2-3] are pending
        int requested_cpus = 3;
        array_cpuid_t_clear(&cpus_priority_array);
        array_cpuid_t_push(&cpus_priority_array, 2);
        array_cpuid_t_push(&cpus_priority_array, 3);
        array_cpuid_t_push(&cpus_priority_array, 0);
        array_cpuid_t_push(&cpus_priority_array, 1);
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_cpus,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO,
                    /* max_parallelism */ 0, /* last_borrow */ NULL, &tasks)
                == async ? DLB_NOTED : DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 lends CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &tasks) == DLB_SUCCESS );
        if (async) {
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 0 );
        } else {
            assert( tasks.count == 0 );
            assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(3, free_cpus) );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }

        // If polling, process 2 needs to ask again for CPU 3
        if (polling) {
            assert( shmem_cpuinfo__acquire_cpu(p2_pid, 3, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 3
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }

        // Finalize
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
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

        // Update pointers of the new shmem
        free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
        occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

        // P2 lends CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(2, free_cpus) );
        assert( CPU_COUNT(occupied_cores) == 0 );

        // P1 acquires CPU 2
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 2, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 1 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(2, occupied_cores) );

        // P2 reclaims CPU 2 and requests CPUs 0 and 1
        assert( shmem_cpuinfo__reclaim_cpu(p2_pid, 2, &tasks) == DLB_NOTED );
        assert( tasks.count == 2 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 2
                && tasks.items[1].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);
        err = shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );
        err = shmem_cpuinfo__acquire_cpu(p2_pid, 1, &tasks);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        assert( tasks.count == 0 );
        assert( CPU_COUNT(free_cpus) == 0 );
        assert( CPU_COUNT(occupied_cores) == 1 && CPU_ISSET(2, occupied_cores) );

        // P1 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        if (async) {
            // P2 gets CPUs 0-2
            assert( tasks.count == 3 );
            for (int i = 0; i < 3; ++i) {
                assert( tasks.items[i].pid == p2_pid
                        && tasks.items[i].cpuid == i
                        && tasks.items[i].action == ENABLE_CPU );
            }
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 0 );
        } else {
            // P2 gets CPU 2
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 2
                    && tasks.items[0].action == ENABLE_CPU );
            assert( CPU_COUNT(free_cpus) == 2
                    && CPU_ISSET(0, free_cpus)
                    && CPU_ISSET(1, free_cpus) );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }
        array_cpuinfo_task_t_clear(&tasks);
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );

        // If polling, P2 needs to ask again for CPUs 0-2
        if (polling) {
            assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 2) == DLB_SUCCESS );
            assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 0
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( shmem_cpuinfo__acquire_cpu(p2_pid, 1, &tasks) == DLB_SUCCESS );
            assert( tasks.count == 1 );
            assert( tasks.items[0].pid == p2_pid
                    && tasks.items[0].cpuid == 1
                    && tasks.items[0].action == ENABLE_CPU );
            array_cpuinfo_task_t_clear(&tasks);
            assert( CPU_COUNT(free_cpus) == 0 );
            assert( CPU_COUNT(occupied_cores) == 0 );
        }

        // P2 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    mu_finalize();

    return 0;
}
