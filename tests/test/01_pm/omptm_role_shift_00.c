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

#include "LB_numThreads/omptm_role_shift.h"
#include "LB_numThreads/omptool.h"
#include "LB_numThreads/omp-tools.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
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

typedef enum CPUStatus {
    OWN      = 0,
    UNKNOWN  = 1 << 0,
    LENT     = 1 << 1,
    BORROWED = 1 << 2
} cpu_status_t;

typedef struct DLB_ALIGN_CACHE CPU_Data {
    _Atomic(cpu_status_t) ownership;
    bool         fa;
} cpu_data_t;

enum {
    PARALLEL_UNSET,
    PARALLEL_LEVEL_1,
};

// This test needs at least room for 8 CPUs
enum { SYS_SIZE = 8 };

static int thread_id = 0;
int __kmp_get_thread_id(void) {
    return thread_id;
}

/*int __kmp_get_num_free_agent_threads(void) {
    return SYS_SIZE-1;
}*/
int __kmp_get_num_threads_role(ompt_role_t r){
    if(r == OMP_ROLE_FREE_AGENT){
        return atoi(getenv("KMP_FREE_AGENT_NUM_THREADS"));
    }
    return -1;
}

static bool free_agent_threads_status[SYS_SIZE];
/*void __kmp_set_free_agent_thread_active_status(
        unsigned int thread_num, bool active) {
    free_agent_threads_status[thread_num] = active;
}*/
void __kmp_set_thread_roles2(int tid, ompt_role_t r){
    if(tid == SYS_SIZE) tid = thread_id;
    if (r == OMP_ROLE_FREE_AGENT){
        free_agent_threads_status[tid] = true;
    }
    else {
        free_agent_threads_status[tid] = false;
    }
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

    /* P1 inits free agents module with OMP_NUM_THREADS=1 and *
     * KMP_NUM_THREADS=8                                      */
    {
        setenv("OMP_NUM_THREADS", "1", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "8", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( omptm_role_shift_testing__get_num_free_agents() == 8 );
        assert( omptm_role_shift_testing__get_num_registered_threads() == 8 );

        omptm_role_shift__finalize();
    }
    /* P1 inits free agents module with OMP_NUM_THREADS=8 and *
     * KMP_NUM_THREADS=8                                      */
    {
        setenv("OMP_NUM_THREADS", "8", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "8", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( omptm_role_shift_testing__get_num_free_agents() == 8 );
        assert( omptm_role_shift_testing__get_num_registered_threads() == 8 );

        omptm_role_shift__finalize();
    }
    /* P1 inits free agents module with OMP_NUM_THREADS=8 and *
     * KMP_NUM_THREADS=4                                      */
    {
        setenv("OMP_NUM_THREADS", "8", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "4", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( omptm_role_shift_testing__get_num_free_agents() == 4 );
        assert( omptm_role_shift_testing__get_num_registered_threads() == 8 );

        omptm_role_shift__finalize();
    }
    // Initialize thread status array to false. They have not been created yet.
    for (int i=0; i<SYS_SIZE; ++i) {
        free_agent_threads_status[i] = false;
    }

    // Assume OMP_NUM_THREADS=1 for the rest of the test
    setenv("OMP_NUM_THREADS", "1", 1);
    setenv("KMP_FREE_AGENT_NUM_THREADS", "4", 1);
    omptm_role_shift__init(p1_pid, &thread_spd->options);

    int *cpu_by_id = omptm_role_shift_testing__get_cpu_by_id_ptr();
    cpu_data_t *cpu_data = omptm_role_shift_testing__get_cpu_data_ptr();

    /* Emulate initial free agent threads creation based on env variables*/
    {
        for (int i=1; i<=3; ++i) {
            thread_id = i;
            omptm_role_shift__thread_begin(ompt_thread_other);
            free_agent_threads_status[i] = true;
        }
        /* When using the OpenMP runtime properly, each thread will call the callback and
           execute the pthread_getaffinity_np and obtain the unique CPU where the free agents
           are binded. Since there's no runtime here, we modify the variables here */
        cpu_by_id[1] = 1;
        cpu_data[1].fa = true;
        cpu_by_id[2] = 4;
        cpu_data[4].fa = true;
        cpu_by_id[3] = 5;
        cpu_data[5].fa = true;


        /* check all CPU ids */
        assert( cpu_by_id[0] == 0 );
        assert( cpu_by_id[1] == 1 );
        assert( cpu_by_id[2] == 4 );
        assert( cpu_by_id[3] == 5 );
        assert( cpu_by_id[4] == -1 );
        assert( cpu_by_id[5] == -1 );
        assert( cpu_by_id[6] == -1 );
        assert( cpu_by_id[7] == -1 );

        /* check all ids from CPU */
        assert( omptm_role_shift_testing__get_id_from_cpu(0) == 0 );
        assert( omptm_role_shift_testing__get_id_from_cpu(1) == 1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(2) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(3) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(4) == 2 );
        assert( omptm_role_shift_testing__get_id_from_cpu(5) == 3 );
        assert( omptm_role_shift_testing__get_id_from_cpu(6) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(7) == -1 );
    }

    /* Test the task_schedule function when there is no parallel region involved */
    {

        /* Execute 4 pending tasks. There should not be any pending task after that.
           CPUs [2,3,6,7] should be inactive */
        omptm_role_shift_testing__set_pending_tasks(4);
        omptm_role_shift__task_switch();
        omptm_role_shift__task_switch();
        omptm_role_shift__task_switch();
        omptm_role_shift__task_switch();

        assert( omptm_role_shift_testing__get_pending_tasks() == 0 );
        assert( omptm_role_shift_testing__get_id_from_cpu(0) == 0 );
        assert( omptm_role_shift_testing__get_id_from_cpu(1) == 1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(2) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(3) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(4) == 2 );
        assert( omptm_role_shift_testing__get_id_from_cpu(5) == 3 );
        assert( omptm_role_shift_testing__get_id_from_cpu(6) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(7) == -1 );

        /* The four tasks end their execution, one by each thread.
           The process should retain the CPUs from the free agent threads
           since it's the owner and the lewi policy is not aggressive */
        omptm_role_shift_testing__set_global_tid(3);
        omptm_role_shift__task_complete();
        omptm_role_shift_testing__set_global_tid(2);
        omptm_role_shift__task_complete();
        omptm_role_shift_testing__set_global_tid(1);
        omptm_role_shift__task_complete();
        omptm_role_shift_testing__set_global_tid(0);
        omptm_role_shift__task_complete();

        assert( omptm_role_shift_testing__get_id_from_cpu(0) == 0 );
        assert( omptm_role_shift_testing__get_id_from_cpu(1) == 1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(2) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(3) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(4) == 2 );
        assert( omptm_role_shift_testing__get_id_from_cpu(5) == 3 );
        assert( omptm_role_shift_testing__get_id_from_cpu(6) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(7) == -1 );
    }

    /* Test the task_create callback. Creates 4 tasks, ensure that they
       are counted, and check that no CPU is acquired. */
    {
        omptm_role_shift_testing__set_pending_tasks(0);

        omptm_role_shift__task_create();
        assert(omptm_role_shift_testing__get_pending_tasks() == 1);
        omptm_role_shift__task_create();
        assert(omptm_role_shift_testing__get_pending_tasks() == 2);
        omptm_role_shift__task_create();
        assert(omptm_role_shift_testing__get_pending_tasks() == 3);
        omptm_role_shift__task_create();
        assert(omptm_role_shift_testing__get_pending_tasks() == 4);

        assert( omptm_role_shift_testing__get_id_from_cpu(0) == 0 );
        assert( omptm_role_shift_testing__get_id_from_cpu(1) == 1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(2) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(3) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(4) == 2 );
        assert( omptm_role_shift_testing__get_id_from_cpu(5) == 3 );
        assert( omptm_role_shift_testing__get_id_from_cpu(6) == -1 );
        assert( omptm_role_shift_testing__get_id_from_cpu(7) == -1 );

        omptm_role_shift_testing__set_pending_tasks(0);
    }

    /* Check the parallel_begin and parallel_end callbacks. First, check that the
       variables are updated correctly without nesting, and then under nested parallelism. */
    {
        omptool_parallel_data_t omptool_parallel_data = {
            .level = 1,
            .requested_parallelism = 4,
        };
        omptm_role_shift__parallel_begin(&omptool_parallel_data);
        assert(omptm_role_shift_testing__in_parallel());
        assert(omptm_role_shift_testing__get_current_parallel_size() == 4);

        omptm_role_shift__parallel_end(&omptool_parallel_data);
        assert(!omptm_role_shift_testing__in_parallel());

        //Testing a nested parallel
        omptm_role_shift__parallel_begin(&omptool_parallel_data);
        assert(omptm_role_shift_testing__in_parallel());
        assert(omptm_role_shift_testing__get_current_parallel_size() == 4);

        omptool_parallel_data_t omptool_parallel_data_level2 = {
            .level = 2,
            .requested_parallelism = 2,
        };
        omptm_role_shift__parallel_begin(&omptool_parallel_data_level2);
        assert(omptm_role_shift_testing__in_parallel());
        // nested parallel is actually ignored, size is of level 1
        assert(omptm_role_shift_testing__get_current_parallel_size() == 4);

        omptm_role_shift__parallel_end(&omptool_parallel_data_level2);
        assert(omptm_role_shift_testing__in_parallel());

        omptm_role_shift__parallel_end(&omptool_parallel_data);
        assert(!omptm_role_shift_testing__in_parallel());
    }

    //Check basic actions of thread_role_shift callback
    {
        int global_tid = 2;
        omptm_role_shift_testing__set_global_tid(global_tid);
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);

        //FA -> NONE transition, nothing should happen
        omptm_role_shift__thread_role_shift(NULL, 
                OMP_ROLE_FREE_AGENT, OMP_ROLE_NONE);
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);

        //NONE -> FA transition. Nothing should happen because the process owns the CPU
        omptm_role_shift_testing__set_pending_tasks(0);
        omptm_role_shift__thread_role_shift(NULL,
                OMP_ROLE_NONE, OMP_ROLE_FREE_AGENT);
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);
    }

    /* Test IntoBlockingCall and OutOfBlockingCall */
    {
        /* P1 is outside a parallel region, and invokes IntoBlockingCall */
        omptm_role_shift_testing__set_global_tid(0);
        omptm_role_shift__IntoBlockingCall();
        assert(!free_agent_threads_status[1]);
        assert(!free_agent_threads_status[2]);
        assert(!free_agent_threads_status[3]);
        assert(cpu_data[0].ownership == LENT);
        assert(cpu_data[1].ownership == LENT);
        assert(cpu_data[4].ownership == LENT);
        assert(cpu_data[5].ownership == LENT);

        /* P2 asks for all the CPUs, it should success (CPU 0 is not lent)  */
        //TODO: Not sure about CPU 0
        assert( shmem_cpuinfo__borrow_ncpus_from_cpu_subset(p2_pid, NULL,
                    &cpus_priority_array, LEWI_AFFINITY_AUTO, 0,
                    &last_borrow, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 4 );
        assert( tasks.items[0].pid == p2_pid
                && tasks.items[0].cpuid == 0
                && tasks.items[0].action == ENABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 1
                && tasks.items[1].action == ENABLE_CPU );
        assert( tasks.items[2].pid == p2_pid
                && tasks.items[2].cpuid == 4
                && tasks.items[2].action == ENABLE_CPU );
        assert( tasks.items[3].pid == p2_pid
                && tasks.items[3].cpuid == 5
                && tasks.items[3].action == ENABLE_CPU );
        array_cpuinfo_task_t_clear(&tasks);

        /* P2 returns P1's CPUs */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p1_mask, &tasks) == DLB_SUCCESS );
        assert( tasks.count == 0 );

        /* P1 invokes OutOfBlockingCall */
        omptm_role_shift__OutOfBlockingCall();
        assert(free_agent_threads_status[1]);
        assert(free_agent_threads_status[2]);
        assert(free_agent_threads_status[3]);
        assert(cpu_data[0].ownership == OWN);
        assert(cpu_data[1].ownership == OWN);
        assert(cpu_data[4].ownership == OWN);
        assert(cpu_data[5].ownership == OWN);
    }

    /* P2 lends all CPUs. P1 creates tasks and executes them. The CPUs should be used */
    {
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, &tasks) == DLB_SUCCESS );
        omptm_role_shift_testing__set_pending_tasks(0);
        
        //Create two tasks. Free agents should be activated in CPUs 2 and 3
        thread_id = 4;
        omptm_role_shift__task_create();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert(free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);

        thread_id = 5;
        omptm_role_shift__task_create();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);

        assert(omptm_role_shift_testing__get_pending_tasks() == 2);
        omptm_role_shift_testing__set_pending_tasks(10);

        //Schdeule 2 tasks. Two free agents should be created in CPUs 6 and 7

        thread_id = 6;
        omptm_role_shift__task_switch();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);

        thread_id = 7;
        omptm_role_shift__task_switch();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        //Thread 5 finishes a task and there are some pending tasks.
        //The status of the threads should not change.
        omptm_role_shift_testing__set_global_tid(5);
        //Update CPU data manually since thread_begin is not really called for this thread
        cpu_data[cpu_by_id[4]].fa = true;
        cpu_data[cpu_by_id[5]].fa = true;
        cpu_data[cpu_by_id[6]].fa = true;
        cpu_data[cpu_by_id[7]].fa = true;
        omptm_role_shift__task_complete();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        omptm_role_shift_testing__set_pending_tasks(0);
        //A task is finished an there are no pending tasks. 
        //The CPU should be lend to DLB and the state updated
        omptm_role_shift__task_complete();
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        //P2 reclaims its process mask. CPU 3 can be immediately reclaimed
        assert( shmem_cpuinfo__reclaim_all(p2_pid, &tasks) == DLB_NOTED );
        assert( tasks.count == 7 );
        assert( tasks.items[0].pid == p1_pid
                && tasks.items[0].cpuid == 2
                && tasks.items[0].action == DISABLE_CPU );
        assert( tasks.items[1].pid == p2_pid
                && tasks.items[1].cpuid == 2
                && tasks.items[1].action == ENABLE_CPU );
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

        //Threads 4, 6, and 7 (Free agents) finish a task.
        //The CPUs should be disabled and the threads change their role
        omptm_role_shift_testing__set_global_tid(4);
        omptm_role_shift__task_complete();
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(!free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        omptm_role_shift_testing__set_global_tid(6);
        omptm_role_shift__task_complete();
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == BORROWED);
        assert(!free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        omptm_role_shift_testing__set_global_tid(7);
        omptm_role_shift__task_complete();
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert(!free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);
    }

    /* Finalize */
    assert( Finish(thread_spd) == DLB_SUCCESS );
    omptm_role_shift__finalize();
    assert( shmem_cpuinfo__finalize(p2_pid, thread_spd->options.shm_key,
                thread_spd->options.lewi_color) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, thread_spd->options.shm_key,
                thread_spd->options.shm_size_multiplier) == DLB_SUCCESS );

    return 0;
}
