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
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptm_free_agents.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <stdbool.h>
#include <sched.h>
#include <assert.h>

/* !!!: Including c file to invoke static methods and check static variables */
#include "LB_numThreads/omptm_free_agents.c"

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
    printf("p1_options: %s\n", p1_options);
    printf("options: %s\n", options);
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
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, thread_spd->options.shm_key)
            == DLB_SUCCESS );
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, thread_spd->options.shm_key)
            == DLB_SUCCESS );

    // Setup dummy priority CPUs
    int cpus_priority_array[SYS_SIZE];
    int i;
    for (i=0; i<SYS_SIZE; ++i) cpus_priority_array[i] = i;

    pid_t new_guests[SYS_SIZE];
    pid_t victims[SYS_SIZE];
    int64_t last_borrow = 0;

    /* P1 inits free agents module with OMP_NUM_THREADS=1 */
    {
        setenv("OMP_NUM_THREADS", "1", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [1,4,5], [2,3,6,7] */
        assert( get_free_agent_binding(0) == 1 );
        assert( get_free_agent_binding(1) == 4 );
        assert( get_free_agent_binding(2) == 5 );
        assert( get_free_agent_binding(3) == 2 );
        assert( get_free_agent_binding(4) == 3 );
        assert( get_free_agent_binding(5) == 6 );
        assert( get_free_agent_binding(6) == 7 );
        assert( get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    /* P1 inits free agents module with OMP_NUM_THREADS=2 */
    {
        setenv("OMP_NUM_THREADS", "2", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [4,5], [2,3,6,7], [1] */
        assert( get_free_agent_binding(0) == 4 );
        assert( get_free_agent_binding(1) == 5 );
        assert( get_free_agent_binding(2) == 2 );
        assert( get_free_agent_binding(3) == 3 );
        assert( get_free_agent_binding(4) == 6 );
        assert( get_free_agent_binding(5) == 7 );
        assert( get_free_agent_binding(6) == 1 );
        assert( get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    /* P1 inits free agents module with OMP_NUM_THREADS=4 */
    {
        setenv("OMP_NUM_THREADS", "4", 1);
        omptm_free_agents__init(p1_pid, &thread_spd->options);

        /* Free agents should bind in this order: [2,3,6,7], [1,4,5] */
        assert( get_free_agent_binding(0) == 2 );
        assert( get_free_agent_binding(1) == 3 );
        assert( get_free_agent_binding(2) == 6 );
        assert( get_free_agent_binding(3) == 7 );
        assert( get_free_agent_binding(4) == 1 );
        assert( get_free_agent_binding(5) == 4 );
        assert( get_free_agent_binding(6) == 5 );
        assert( get_free_agent_binding(7) == -1 );

        omptm_free_agents__finalize();
    }

    // Initialize thread status array to true just to make sure
    // the thread_begin function disables all the free agent threads
    for (i=0; i<SYS_SIZE-1; ++i) {
        free_agent_threads_status[i] = true;
    }

    // Assume OMP_NUM_THREADS=4 for the rest of the test
    setenv("OMP_NUM_THREADS", "4", 1);
    omptm_free_agents__init(p1_pid, &thread_spd->options);

    /* Emulate free agent threads creation */
    {
        for (i=0; i<SYS_SIZE-1; ++i) {
            free_agent_id = i;
            omptm_free_agents__thread_begin(ompt_thread_other, NULL);
        }

        /* All free agents are disabled */
        for (i=0; i<SYS_SIZE-1; ++i) {
            assert( free_agent_threads_status[i] == false );
        }

        /* check all CPU ids */
        assert( get_free_agent_id_by_cpuid(0) == -1 );
        assert( get_free_agent_id_by_cpuid(1) == 4 );
        assert( get_free_agent_id_by_cpuid(2) == 0 );
        assert( get_free_agent_id_by_cpuid(3) == 1 );
        assert( get_free_agent_id_by_cpuid(4) == 5 );
        assert( get_free_agent_id_by_cpuid(5) == 6 );
        assert( get_free_agent_id_by_cpuid(6) == 2 );
        assert( get_free_agent_id_by_cpuid(7) == 3 );

        /* check all free agent ids */
        for (i=0; i<SYS_SIZE-1; ++i) {
            assert( get_free_agent_cpuid_by_id(i) == get_free_agent_binding(i) );
        }

        /* check ordered CPU list */
        assert( free_agent_cpu_list[0] == 1 );
        assert( free_agent_cpu_list[1] == 4 );
        assert( free_agent_cpu_list[2] == 5 );
        assert( free_agent_cpu_list[3] == 2 );
        assert( free_agent_cpu_list[4] == 3 );
        assert( free_agent_cpu_list[5] == 6 );
        assert( free_agent_cpu_list[6] == 7 );
    }

    /* Test tasks out of parallel, then reclaim CPUs from same process (for parallel)
     * and later from other processes */
    {
        /* Free agents assigned to CPUs [1,4,5] should take priority,
         * since they belong to the process mask and there is not an
         * active parallel region */
        assert( DLB_ATOMIC_LD(&in_parallel) == false );

        /* Acquire CPU 1 */
        acquire_one_free_agent();
        free_agent_id = get_free_agent_id_by_cpuid(1);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (i=0; i<SYS_SIZE; ++i) {
            if (i != 1) {
                free_agent_id = get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* Acquire CPU 4 */
        acquire_one_free_agent();
        free_agent_id = get_free_agent_id_by_cpuid(4);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4) {
                free_agent_id = get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* acquire CPU 5 */
        acquire_one_free_agent();
        free_agent_id = get_free_agent_id_by_cpuid(5);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5) {
                free_agent_id = get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* another acquire should do nothing, since CPUs 2,3,6,7 are still guested by P2 */
        acquire_one_free_agent();
        for (i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5) {
                free_agent_id = get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* P2 lends everything */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );

        /* acquire CPU 2 */
        acquire_one_free_agent();
        free_agent_id = get_free_agent_id_by_cpuid(2);
        assert( free_agent_threads_status[free_agent_id] == true );
        for (i=0; i<SYS_SIZE; ++i) {
            if (i != 1 && i != 4 && i != 5 && i != 2) {
                free_agent_id = get_free_agent_id_by_cpuid(i);
                if (free_agent_id != -1) {
                    assert( free_agent_threads_status[free_agent_id] == false );
                }
            }
        }

        /* acquire CPUs 3,6,7 */
        acquire_one_free_agent();
        acquire_one_free_agent();
        acquire_one_free_agent();
        for (i=0; i<SYS_SIZE-1; ++i) {
            assert( free_agent_threads_status[i] == true );
        }

        /* P1 starts a parallel region with 4 threads */
        ompt_frame_t encountering_task_frame = {.exit_frame.ptr = NULL };
        ompt_data_t parallel_data;
        omptm_free_agents__parallel_begin(
                NULL, &encountering_task_frame, &parallel_data,
                4, ompt_parallel_team, NULL);
        __worker_binding = 0;
        omptm_free_agents__implicit_task(
                ompt_scope_begin, &parallel_data, NULL, 4, 0, 0);
        __worker_binding = 1;
        omptm_free_agents__implicit_task(
                ompt_scope_begin, &parallel_data, NULL, 4, 1, 0);
        __worker_binding = 4;
        omptm_free_agents__implicit_task(
                ompt_scope_begin, &parallel_data, NULL, 4, 2, 0);
        __worker_binding = 5;
        omptm_free_agents__implicit_task(
                ompt_scope_begin, &parallel_data, NULL, 4, 3, 0);
        assert( DLB_ATOMIC_LD(&cpu_data[0].state) & CPU_STATE_IN_PARALLEL );
        assert( DLB_ATOMIC_LD(&cpu_data[1].state) & CPU_STATE_IN_PARALLEL );
        assert( DLB_ATOMIC_LD(&cpu_data[4].state) & CPU_STATE_IN_PARALLEL );
        assert( DLB_ATOMIC_LD(&cpu_data[5].state) & CPU_STATE_IN_PARALLEL );

         /* free agents in CPUs 1,4,5 should be disabled */
        free_agent_id = get_free_agent_id_by_cpuid(1);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = get_free_agent_id_by_cpuid(4);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = get_free_agent_id_by_cpuid(5);
        assert( free_agent_threads_status[free_agent_id] == false );

        /* free agents in CPUs 2,3,6,7 should not */
        assert( num_enabled_free_agents == 4 );
        free_agent_id = get_free_agent_id_by_cpuid(2);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = get_free_agent_id_by_cpuid(3);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = get_free_agent_id_by_cpuid(6);
        assert( free_agent_threads_status[free_agent_id] == true );
        free_agent_id = get_free_agent_id_by_cpuid(7);
        assert( free_agent_threads_status[free_agent_id] == true );

        /* free agent in CPU 2 finishes a task, let's assume there are some pending tasks */
        pending_tasks = 10;
        assert( num_enabled_free_agents == 4 );
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(2);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == true );
        assert( num_enabled_free_agents == 4 );

        /* same, but now without pending tasks */
        pending_tasks = 0;
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(2);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( num_enabled_free_agents == 3 );

        /* P2 reclaims its process mask, CPU 2 can be immediately acquired */
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid
                && new_guests[6] == p2_pid && new_guests[7] == p2_pid );
        assert( victims[2] == -1 && victims[3] == p1_pid
                && victims[6] == p1_pid && victims[7] == p1_pid );

        /* Free agents finish their tasks, they should notice their CPU is reclaimed */
        pending_tasks = 10;
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(3);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(6);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(7);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( num_enabled_free_agents == 0 );

        /* P1 starts a nested parallel region, it should not affect anything */
        ompt_frame_t encountering_task_frame_level2 = {.exit_frame.ptr = &main };
        ompt_data_t parallel_data_level2;
        omptm_free_agents__parallel_begin(
                NULL, &encountering_task_frame_level2, &parallel_data_level2,
                1, ompt_parallel_team, NULL);
        omptm_free_agents__implicit_task(
                ompt_scope_begin, &parallel_data_level2, NULL, 1, 0, 0);
        assert( num_enabled_free_agents == 0 );
        acquire_one_free_agent();
        assert( num_enabled_free_agents == 0 );

        /* P1 finalizes nested parallel, nothing... */
        omptm_free_agents__parallel_end(&parallel_data_level2, NULL, 0, NULL);
        assert( DLB_ATOMIC_LD(&in_parallel) == true );
        assert( num_enabled_free_agents == 0 );
        acquire_one_free_agent();
        assert( num_enabled_free_agents == 0 );

        /* P1 finalizes level 1 parallel */
        omptm_free_agents__parallel_end(&parallel_data, NULL, 0, NULL);
        assert( DLB_ATOMIC_LD(&in_parallel) == false );
        assert( num_enabled_free_agents == 0 );
        assert( DLB_ATOMIC_LD(&cpu_data[1].state) & CPU_STATE_IDLE );
        assert( DLB_ATOMIC_LD(&cpu_data[4].state) & CPU_STATE_IDLE );
        assert( DLB_ATOMIC_LD(&cpu_data[5].state) & CPU_STATE_IDLE );
        acquire_one_free_agent();
        acquire_one_free_agent();
        acquire_one_free_agent();
        assert( num_enabled_free_agents == 3 );
        assert( DLB_ATOMIC_LD(&cpu_data[1].state) & CPU_STATE_FREE_AGENT_ENABLED );
        assert( DLB_ATOMIC_LD(&cpu_data[4].state) & CPU_STATE_FREE_AGENT_ENABLED );
        assert( DLB_ATOMIC_LD(&cpu_data[5].state) & CPU_STATE_FREE_AGENT_ENABLED );

        /* No more tasks, disable free agents */
        pending_tasks = 0;
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(1);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( num_enabled_free_agents == 2 );
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(4);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( num_enabled_free_agents == 1 );
        free_agent_id = __free_agent_id = get_free_agent_id_by_cpuid(5);
        omptm_free_agents__task_schedule(NULL, ompt_task_complete, NULL);
        assert( free_agent_threads_status[free_agent_id] == false );
        assert( num_enabled_free_agents == 0 );
        assert( DLB_ATOMIC_LD(&cpu_data[1].state) & CPU_STATE_IDLE );
        assert( DLB_ATOMIC_LD(&cpu_data[4].state) & CPU_STATE_IDLE );
        assert( DLB_ATOMIC_LD(&cpu_data[5].state) & CPU_STATE_IDLE );
    }

    /* Test IntoBlockingCall and OutOfBlockingCall: easy, no reclaim */
    {
        /* P1 is outside a parallel region, and invokes IntoBlockingCall */
        omptm_free_agents__IntoBlockingCall();

        /* P2 asks for all the CPUs, it should success (CPU 0 is not lent)  */
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p2_pid, NULL,
                    cpus_priority_array, PRIO_ANY, 0,
                    &last_borrow, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1 && new_guests[1] == p2_pid
                && new_guests[4] == p2_pid && new_guests[5] == p2_pid );

        /* P2 returns P1's CPUs */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p1_mask, new_guests) == DLB_SUCCESS );

        /* P1 invokes OutOfBlockingCall */
        assert( num_enabled_free_agents == 0 );
        omptm_free_agents__OutOfBlockingCall();
        assert( num_enabled_free_agents == 0 );

        /* Free agents can be enabled on CPUs 1,4,5 */
        pending_tasks = 10;
        assert( num_enabled_free_agents == 0 );
        acquire_one_free_agent();
        acquire_one_free_agent();
        acquire_one_free_agent();
        assert( num_enabled_free_agents == 3 );
    }

    /* Finalize */
    assert( Finish(thread_spd) == DLB_SUCCESS );
    omptm_free_agents__finalize();
    assert( shmem_cpuinfo__finalize(p2_pid, thread_spd->options.shm_key) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, thread_spd->options.shm_key) == DLB_SUCCESS );

    return 0;
}
