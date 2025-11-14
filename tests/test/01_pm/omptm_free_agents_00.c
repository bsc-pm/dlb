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

#include "LB_numThreads/omptm_free_agents.h"
#include "LB_numThreads/omptool.h"
#include "LB_numThreads/omp-tools.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <assert.h>
#include <sched.h>
#include <stdbool.h>
#include <string.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

// This test needs at least room for 8 CPUs
enum { SYS_SIZE = 8 };

static int free_agent_id = 0;
int __kmp_get_free_agent_id(void) {
    return free_agent_id;
}

int __kmp_get_num_free_agent_threads(void) {
    return SYS_SIZE-1;
}

static bool free_agent_threads_status[SYS_SIZE-1];
void __kmp_set_free_agent_thread_active_status(
        unsigned int thread_num, bool active) {
    free_agent_threads_status[thread_num] = active;
}


int main (int argc, char *argv[]) {
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    char options[64] = "--lewi --shm-key=";
    strcat(options, SHMEM_KEY);

    /* P1 needs pre-init + init */
    char p1_options[64] = "--preinit-pid=111 ";
    strcat(p1_options, options);
    cpu_set_t p1_mask;
    mu_parse_mask("0,1,4,5", &p1_mask);
    spd_enter_dlb(NULL);
    thread_spd->dlb_initialized = true;
    assert( PreInitialize(thread_spd, &p1_mask, p1_options) == DLB_SUCCESS );
    pid_t p1_pid = 111;
    assert( Initialize(thread_spd, p1_pid, CPU_COUNT(&p1_mask), &p1_mask, p1_options)
            == DLB_SUCCESS );

    /* P2 is fake and does nothing with OMPT, just init shmems */
    cpu_set_t p2_mask;
    mu_parse_mask("2,3,6,7", &p2_mask);
    pid_t p2_pid = 222;
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, thread_spd->options.shm_key,
                thread_spd->options.lewi_color) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, thread_spd->options.shm_key,
                thread_spd->options.shm_size_multiplier) == DLB_SUCCESS );

    // Setup dummy priority CPUs
    array_cpuid_t cpus_priority_array;
    array_cpuid_t_init(&cpus_priority_array, SYS_SIZE);
    for (int i=0; i<SYS_SIZE; ++i) array_cpuid_t_push(&cpus_priority_array, i);

    array_cpuinfo_task_t tasks;
    array_cpuinfo_task_t_init(&tasks, SYS_SIZE);

    int64_t last_borrow = 0;

    /* P1 inits free agents module with OMP_NUM_THREADS=1 */
    {
        setenv("OMP_NUM_THREADS", "1", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [1,4,5], [2,3,6,7] */
        assert( omptm_free_agents_testing__get_free_agent_binding(0) == 1 );
        assert( omptm_free_agents_testing__get_free_agent_binding(1) == 4 );
        assert( omptm_free_agents_testing__get_free_agent_binding(2) == 5 );
        assert( omptm_free_agents_testing__get_free_agent_binding(3) == 2 );
        assert( omptm_free_agents_testing__get_free_agent_binding(4) == 3 );
        assert( omptm_free_agents_testing__get_free_agent_binding(5) == 6 );
        assert( omptm_free_agents_testing__get_free_agent_binding(6) == 7 );
        assert( omptm_free_agents_testing__get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    /* P1 inits free agents module with OMP_NUM_THREADS=2 */
    {
        setenv("OMP_NUM_THREADS", "2", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [4,5], [2,3,6,7], [1] */
        assert( omptm_free_agents_testing__get_free_agent_binding(0) == 4 );
        assert( omptm_free_agents_testing__get_free_agent_binding(1) == 5 );
        assert( omptm_free_agents_testing__get_free_agent_binding(2) == 2 );
        assert( omptm_free_agents_testing__get_free_agent_binding(3) == 3 );
        assert( omptm_free_agents_testing__get_free_agent_binding(4) == 6 );
        assert( omptm_free_agents_testing__get_free_agent_binding(5) == 7 );
        assert( omptm_free_agents_testing__get_free_agent_binding(6) == 1 );
        assert( omptm_free_agents_testing__get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    /* P1 inits free agents module with OMP_NUM_THREADS=4 */
    {
        setenv("OMP_NUM_THREADS", "4", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [2,3,6,7], [1,4,5] */
        assert( omptm_free_agents_testing__get_free_agent_binding(0) == 2 );
        assert( omptm_free_agents_testing__get_free_agent_binding(1) == 3 );
        assert( omptm_free_agents_testing__get_free_agent_binding(2) == 6 );
        assert( omptm_free_agents_testing__get_free_agent_binding(3) == 7 );
        assert( omptm_free_agents_testing__get_free_agent_binding(4) == 1 );
        assert( omptm_free_agents_testing__get_free_agent_binding(5) == 4 );
        assert( omptm_free_agents_testing__get_free_agent_binding(6) == 5 );
        assert( omptm_free_agents_testing__get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    // Initialize thread status array to true just to make sure
    // the thread_begin function disables all the free agent threads
    for (int i=0; i<SYS_SIZE-1; ++i) {
        free_agent_threads_status[i] = true;
    }

    // Assume OMP_NUM_THREADS=4 for the rest of the test
    setenv("OMP_NUM_THREADS", "4", 1);
    omptm_free_agents__init(p1_pid, &thread_spd->options);

    /* Emulate free agent threads creation */
    {
        for (int i=0; i<SYS_SIZE-1; ++i) {
            free_agent_id = i;
            omptm_free_agents__thread_begin(ompt_thread_other);
        }

        /* All free agents are disabled */
        for (int i=0; i<SYS_SIZE-1; ++i) {
            assert( free_agent_threads_status[i] == false );
        }

        /* check all CPU ids */
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(0) == -1 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(1) == 4 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(2) == 0 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(3) == 1 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(4) == 5 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(5) == 6 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(6) == 2 );
        assert( omptm_free_agents_testing__get_free_agent_id_by_cpuid(7) == 3 );

        /* check all free agent ids */
        for (int i=0; i<SYS_SIZE-1; ++i) {
            assert( omptm_free_agents_testing__get_free_agent_cpuid_by_id(i)
                    == omptm_free_agents_testing__get_free_agent_binding(i) );
        }

        /* check ordered CPU list */
        assert( omptm_free_agents_testing__get_free_agent_cpu(0) == 1 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(1) == 4 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(2) == 5 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(3) == 2 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(4) == 3 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(5) == 6 );
        assert( omptm_free_agents_testing__get_free_agent_cpu(6) == 7 );
    }

    /* Test tasks out of parallel, then reclaim CPUs from same process (for parallel)
     * and later from other processes */
    {
        /* Free agents assigned to CPUs [1,4,5] should take priority,
         * since they belong to the process mask and there is not an
         * active parallel region */
        assert( !omptm_free_agents_testing__in_parallel() );

        /* Acquire CPU 1 */
        omptm_free_agents_testing__acquire_one_free_agent();
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(1);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (int i=0; i<SYS_SIZE; ++i) {
            if (i != 1) {
                free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* Acquire CPU 4 */
        omptm_free_agents_testing__acquire_one_free_agent();
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(4);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (int i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4) {
                free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* acquire CPU 5 */
        omptm_free_agents_testing__acquire_one_free_agent();
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(5);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (int i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5) {
                free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* another acquire should do nothing, since CPUs 2,3,6,7 are still guested by P2 */
        omptm_free_agents_testing__acquire_one_free_agent();
        for (int i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5) {
                free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* P2 lends everything */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        /* acquire CPU 2 */
        omptm_free_agents_testing__acquire_one_free_agent();
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(2);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (int i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5 && i != 2) {
                free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* acquire CPUs 3,6,7 */
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        for (int i=0; i<SYS_SIZE-1; ++i) {
            assert( free_agent_threads_status[i] == true );
        }

        /* P1 starts a parallel region with 4 threads */
        omptool_parallel_data_t omptool_parallel_data = {
            .level = 1,
            .requested_parallelism = 4,
        };
        omptm_free_agents__parallel_begin(&omptool_parallel_data);
        omptm_free_agents_testing__set_worker_binding(0);
        omptm_free_agents__into_parallel_function(&omptool_parallel_data, 0);
        omptm_free_agents_testing__set_worker_binding(1);
        omptm_free_agents__into_parallel_function(&omptool_parallel_data, 1);
        omptm_free_agents_testing__set_worker_binding(4);
        omptm_free_agents__into_parallel_function(&omptool_parallel_data, 2);
        omptm_free_agents_testing__set_worker_binding(5);
        omptm_free_agents__into_parallel_function(&omptool_parallel_data, 3);
        assert( omptm_free_agents_testing__check_cpu_in_parallel(0) );
        assert( omptm_free_agents_testing__check_cpu_in_parallel(1) );
        assert( omptm_free_agents_testing__check_cpu_in_parallel(4) );
        assert( omptm_free_agents_testing__check_cpu_in_parallel(5) );

         /* free agents in CPUs 1,4,5 should be disabled */
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(1);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(4);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(5);
        assert( free_agent_threads_status[free_agent_id] == false );

        /* free agents in CPUs 2,3,6,7 should not */
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 4 );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(2);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(3);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(6);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(7);
        assert( free_agent_threads_status[free_agent_id] == true );

        /* free agent in CPU 2 finishes a task, let's assume there are some pending tasks */
        omptm_free_agents_testing__set_pending_tasks(10);
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 4 );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(2);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == true );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 4 );

        /* same, but now without pending tasks */
        omptm_free_agents_testing__set_pending_tasks(0);
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(2);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 3 );

        /* P2 reclaims its process mask, CPU 2 can be immediately acquired */
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 7 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p1_pid
                && tasks.items[1].cpuid == 3
                && tasks.items[1].action == DISABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 3
                && tasks.items[2].action == ENABLE_CPU );
        assert( tasks.items[3].pid == p1_pid
                && tasks.items[3].cpuid == 6
                && tasks.items[3].action == DISABLE_CPU );
        assert( tasks.items[4].pid == p2_pid
                && tasks.items[4].cpuid == 6
                && tasks.items[4].action == ENABLE_CPU );
        assert( tasks.items[5].pid == p1_pid
                && tasks.items[5].cpuid == 7
                && tasks.items[5].action == DISABLE_CPU );
        assert( tasks.items[6].pid == p2_pid
                && tasks.items[6].cpuid == 7
                && tasks.items[6].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        /* Free agents finish their tasks, they should notice their CPU is reclaimed */
        omptm_free_agents_testing__set_pending_tasks(10);
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(3);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(6);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(7);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );

        /* P1 starts a nested parallel region, it should not affect anything */
        omptool_parallel_data_t omptool_parallel_data_level2 = {
            .level = 2,
            .requested_parallelism = 1,
        };
        omptm_free_agents__parallel_begin(&omptool_parallel_data_level2);
        omptm_free_agents__into_parallel_function(&omptool_parallel_data_level2, 0);
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        omptm_free_agents_testing__acquire_one_free_agent();
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );

        /* P1 finalizes nested parallel, nothing... */
        omptm_free_agents__parallel_end(&omptool_parallel_data_level2);
        assert( omptm_free_agents_testing__in_parallel() );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        omptm_free_agents_testing__acquire_one_free_agent();
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );

        /* P1 finalizes level 1 parallel */
        omptm_free_agents__parallel_end(&omptool_parallel_data);
        assert( !omptm_free_agents_testing__in_parallel() );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        assert( omptm_free_agents_testing__check_cpu_idle(1) );
        assert( omptm_free_agents_testing__check_cpu_idle(4) );
        assert( omptm_free_agents_testing__check_cpu_idle(5) );
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 3 );
        assert( omptm_free_agents_testing__check_cpu_free_agent_enabled(1) );
        assert( omptm_free_agents_testing__check_cpu_free_agent_enabled(4) );
        assert( omptm_free_agents_testing__check_cpu_free_agent_enabled(5) );

        /* No more tasks, disable free agents */
        omptm_free_agents_testing__set_pending_tasks(0);
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(1);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 2 );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(4);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 1 );
        free_agent_id = omptm_free_agents_testing__get_free_agent_id_by_cpuid(5);
        omptm_free_agents_testing__set_free_agent_id(free_agent_id);
        omptm_free_agents__task_complete();

        assert( free_agent_threads_status[free_agent_id] == false );
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        assert( omptm_free_agents_testing__check_cpu_idle(1) );
        assert( omptm_free_agents_testing__check_cpu_idle(4) );
        assert( omptm_free_agents_testing__check_cpu_idle(5) );
    }

    /* Test IntoBlockingCall and OutOfBlockingCall: easy, no reclaim */
    {
        /* P1 is outside a parallel region, and invokes IntoBlockingCall */
        omptm_free_agents__IntoBlockingCall();

        /* P2 asks for all the CPUs, it should success (CPU 0 is not lent)  */
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p2_pid, NULL,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0,
                    &last_borrow, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 3 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 1
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 4
                && tasks.items[1].action == ENABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 5
                && tasks.items[2].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        /* P2 returns P1's CPUs */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p1_mask, &tasks) == DLB_SUCCESS );

        /* P1 invokes OutOfBlockingCall */
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        omptm_free_agents__OutOfBlockingCall();
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );

        /* Free agents can be enabled on CPUs 1,4,5 */
        omptm_free_agents_testing__set_pending_tasks(10);
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 0 );
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        omptm_free_agents_testing__acquire_one_free_agent();
        assert( omptm_free_agents_testing__get_num_enabled_free_agents() == 3 );
    }

    /* Finalize */
    assert( Finish(thread_spd) == DLB_SUCCESS );
    omptm_free_agents__finalize();
    assert( shmem_cpuinfo__finalize(p2_pid, thread_spd->options.shm_key,
                thread_spd->options.lewi_color) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, thread_spd->options.shm_key,
                thread_spd->options.shm_size_multiplier) == DLB_SUCCESS );

    mu_finalize();

    return 0;
}
