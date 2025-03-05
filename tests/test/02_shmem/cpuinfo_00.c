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
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/types.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

// Basic checks with 1 sub-process

int main( int argc, char **argv ) {
    pid_t pid = getpid();
    cpu_set_t process_mask, mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    int mycpu = sched_getcpu();
    printf("mycpu: %d\n", mycpu);
    int system_size = mu_get_system_size();
    pid_t *new_guests = malloc(sizeof(pid_t) * system_size);
    pid_t *victims = malloc(sizeof(pid_t) * system_size);
    array_cpuinfo_task_t tasks;
    array_cpuinfo_task_t_init(&tasks, system_size);
    array_cpuid_t cpus_priority_array;
    array_cpuid_t_init(&cpus_priority_array, system_size);
    int64_t last_borrow = 0;
    int requested_ncpus = 0;

    // FIXME: this test assumes that the system's mask is 0..system_size
    //        skip otherwise
    if (CPU_COUNT(&process_mask) != system_size) {
        return 0;
    }

    // Setup dummy priority CPUs
    for (int i=0; i<system_size; ++i) {
        if (CPU_ISSET(i, &process_mask)) {
            array_cpuid_t_push(&cpus_priority_array, i);
        }
    }

    // Init
    assert( shmem_cpuinfo__init(pid, 0, &process_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

    // Get pointers to the shmem auxiliary cpu sets
    const cpu_set_t *free_cpus = shmem_cpuinfo_testing__get_free_cpu_set();
    const cpu_set_t *occupied_cores = shmem_cpuinfo_testing__get_occupied_core_set();

    /* Tests using mycpu */

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(mycpu, free_cpus) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Reclaim CPU
    assert( shmem_cpuinfo__reclaim_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 1 );
    assert( tasks.items[0].pid == pid
            && tasks.items[0].cpuid == mycpu
            && tasks.items[0].action == ENABLE_CPU );
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(mycpu, free_cpus) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Reclaim CPUs
    assert( shmem_cpuinfo__reclaim_cpus(pid, 1, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 1 );
    assert( tasks.items[0].pid == pid
            && tasks.items[0].cpuid == mycpu
            && tasks.items[0].action == ENABLE_CPU );
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(mycpu, free_cpus) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Acquire CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 1 );
    assert( tasks.items[0].pid == pid
            && tasks.items[0].cpuid == mycpu
            && tasks.items[0].action == ENABLE_CPU );
    array_cpuinfo_task_t_clear(&tasks);
    assert( shmem_cpuinfo__acquire_cpu(pid, mycpu, &tasks) == DLB_NOUPDT );
    assert( tasks.count == 0 );
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_COUNT(free_cpus) == 1 && CPU_ISSET(mycpu, free_cpus) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Borrow CPUs
    requested_ncpus = 1;
    assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(pid, &requested_ncpus,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                &last_borrow, &tasks)
            == DLB_SUCCESS );
    assert( tasks.count == 1 );
    assert( tasks.items[0].pid == pid
            && tasks.items[0].cpuid == mycpu
            && tasks.items[0].action == ENABLE_CPU );
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    /* Tests using masks */

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_EQUAL(free_cpus, &process_mask) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Reclaim mask
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &process_mask, &tasks) >= 0 );
    assert( tasks.count == (unsigned)system_size );
    for (int i=0; i<system_size; ++i) {
        assert( tasks.items[i].pid == pid
                && tasks.items[i].cpuid == i
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_EQUAL(free_cpus, &process_mask) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Reclaim all (all CPUs in system)
    assert( shmem_cpuinfo__reclaim_all(pid, &tasks) >= 0 );
    assert( tasks.count == (unsigned)system_size );
    for (int i=0; i<system_size; ++i) {
        assert( tasks.items[i].pid == pid
                && tasks.items[i].cpuid == i
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_EQUAL(free_cpus, &process_mask) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Acquire mask
    assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(pid, NULL /* requested_ncpus */,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                &last_borrow, &tasks) >= 0 );
    assert( tasks.count == (unsigned)system_size );
    for (int i=0; i<system_size; ++i) {
        assert( tasks.items[i].pid == pid
                /* && tasks.items[i].cpuid == i */          // this depends on the HW
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );
    assert( CPU_EQUAL(free_cpus, &process_mask) );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Borrow all
    assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(pid, NULL /* requested_ncpus */,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                &last_borrow, &tasks) == DLB_SUCCESS );
    assert( tasks.count == (unsigned)system_size );
    for (int i=0; i<system_size; ++i) {
        assert( tasks.items[i].pid == pid
                /* && tasks.items[i].cpuid == i */          // this depends on the HW
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);
    assert( CPU_COUNT(free_cpus) == 0 );
    assert( CPU_COUNT(occupied_cores) == 0 );

    // Return
    assert( shmem_cpuinfo__return_all(pid, &tasks) == DLB_NOUPDT );
    assert( tasks.count == 0 );
    assert( shmem_cpuinfo__return_cpu(pid, mycpu, &tasks) == DLB_NOUPDT );
    assert( tasks.count == 0 );
    assert( shmem_cpuinfo__return_cpu_mask(pid, &process_mask, &tasks) == DLB_NOUPDT );
    assert( tasks.count == 0 );

    // MaxParallelism
    for (int max=1; max<system_size; ++max) {
        // Set up 'max' CPUs
        assert( shmem_cpuinfo__update_max_parallelism(pid, max, &tasks) == DLB_SUCCESS );
        assert( tasks.count == (unsigned)(system_size - max) );
        for (unsigned i=0; i<tasks.count; ++i) {
            assert( tasks.items[i].pid == pid
                    && tasks.items[i].cpuid == i+max
                    && tasks.items[i].action == DISABLE_CPU );
        }
        array_cpuinfo_task_t_clear(&tasks);
        // Acquire all CPUs again
        assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(pid, NULL /* requested_ncpus */,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                    &last_borrow, &tasks) == DLB_SUCCESS );
        array_cpuinfo_task_t_clear(&tasks);
    }

    // Check errors with a nonexistent CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, system_size, &tasks) == DLB_ERR_PERM );
    assert( tasks.count == 0 );
    assert( shmem_cpuinfo__reclaim_cpu(pid, system_size, &tasks) == DLB_ERR_PERM );
    assert( tasks.count == 0 );
    CPU_ZERO(&mask);
    CPU_SET(system_size, &mask);
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &mask, &tasks) == DLB_NOUPDT );
    assert( tasks.count == 0 );

    // Deregister process and check that no action is needed
    assert( shmem_cpuinfo__deregister(pid, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 0 );

    // Finalize
    assert( shmem_cpuinfo__finalize(pid, SHMEM_KEY, 0) == DLB_SUCCESS );

    /* Test inherit CPUs from preinit pid */
    {
        pid_t preinit_pid = pid+1;
        assert( shmem_cpuinfo_ext__init(SHMEM_KEY, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo_ext__preinit(preinit_pid, &process_mask, DLB_DROM_FLAGS_NONE)
                == DLB_SUCCESS );
        assert( shmem_cpuinfo_ext__finalize() == DLB_SUCCESS );

        assert( shmem_cpuinfo__init(pid, preinit_pid, &process_mask, SHMEM_KEY, 0) == DLB_SUCCESS );

        // Lend mask
        assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        // Reclaim mask
        assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &process_mask, &tasks) >= 0 );
        assert( tasks.count == (unsigned)system_size );
        for (int i=0; i<system_size; ++i) {
            assert( tasks.items[i].pid == pid
                    && tasks.items[i].cpuid == i
                    && tasks.items[i].action == ENABLE_CPU );
        }
        array_cpuinfo_task_t_clear(&tasks);

        assert( shmem_cpuinfo__finalize(pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    /* Test registering with empty mask */
    {
        CPU_ZERO(&process_mask);
        assert( shmem_cpuinfo__init(pid, 0, &process_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        shmem_cpuinfo__print_info(SHMEM_KEY, 0, 0, DLB_COLOR_AUTO);
        assert( shmem_cpuinfo__finalize(pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    }

    free(new_guests);
    free(victims);
    array_cpuid_t_destroy(&cpus_priority_array);

    return 0;
}
