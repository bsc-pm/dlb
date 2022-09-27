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
#include "LB_numThreads/omptm_role_shift.h"
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
#include "LB_numThreads/omptm_role_shift.c"

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

    /* P1 inits free agents module with OMP_NUM_THREADS=1 and *
     * KMP_NUM_THREADS=8                                      */
    {
        setenv("OMP_NUM_THREADS", "1", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "8", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( primary_thread_cpu == 0 );
        assert( num_free_agents == 8 );
        assert( registered_threads == 8 );

        omptm_role_shift__finalize();
    }
    /* P1 inits free agents module with OMP_NUM_THREADS=8 and *
     * KMP_NUM_THREADS=8                                      */
    {
        setenv("OMP_NUM_THREADS", "8", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "8", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( primary_thread_cpu == 0 );
        assert( num_free_agents == 8 );
        assert( registered_threads == 8 );

        omptm_role_shift__finalize();
    }
    /* P1 inits free agents module with OMP_NUM_THREADS=8 and *
     * KMP_NUM_THREADS=4                                      */
    {
        setenv("OMP_NUM_THREADS", "8", 1);
        setenv("KMP_FREE_AGENT_NUM_THREADS", "4", 1);
        omptm_role_shift__init(p1_pid, &thread_spd->options);

        assert( primary_thread_cpu == 0 );
        assert( num_free_agents == 4 );
        assert( registered_threads == 8 );

        omptm_role_shift__finalize();
    }
    // Initialize thread status array to false. They have not been created yet.
    for (i=0; i<SYS_SIZE; ++i) {
        free_agent_threads_status[i] = false;
    }

    // Assume OMP_NUM_THREADS=1 for the rest of the test
    setenv("OMP_NUM_THREADS", "1", 1);
    setenv("KMP_FREE_AGENT_NUM_THREADS", "4", 1);
    omptm_role_shift__init(p1_pid, &thread_spd->options);

    /* Emulate initial free agent threads creation based on env variables*/
    {
        for (i=1; i<=3; ++i) {
            thread_id = i;
            omptm_role_shift__thread_begin(ompt_thread_other, NULL);
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
        assert( get_id_from_cpu(0) == 0 );
        assert( get_id_from_cpu(1) == 1 );
        assert( get_id_from_cpu(2) == -1 );
        assert( get_id_from_cpu(3) == -1 );
        assert( get_id_from_cpu(4) == 2 );
        assert( get_id_from_cpu(5) == 3 );
        assert( get_id_from_cpu(6) == -1 );
        assert( get_id_from_cpu(7) == -1 );
    }

    /* Test the task_schedule function when there is no parallel region involved */
    {

        /* Execute 4 pending tasks. There should not be any pending task after that.
           CPUs [2,3,6,7] should be inactive */
        pending_tasks = 4;
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
        
        assert( pending_tasks == 0 );
        assert( get_id_from_cpu(0) == 0 );
        assert( get_id_from_cpu(1) == 1 );
        assert( get_id_from_cpu(2) == -1 );
        assert( get_id_from_cpu(3) == -1 );
        assert( get_id_from_cpu(4) == 2 );
        assert( get_id_from_cpu(5) == 3 );
        assert( get_id_from_cpu(6) == -1 );
        assert( get_id_from_cpu(7) == -1 );
       
        /* The four tasks end their execution, one by each thread.
           The process should retain the CPUs from the free agent threads
           since it's the owner and the lewi policy is not aggressive */
        global_tid = 3;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        global_tid = 2;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        global_tid = 1;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        global_tid = 0;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);

        assert( get_id_from_cpu(0) == 0 );
        assert( get_id_from_cpu(1) == 1 );
        assert( get_id_from_cpu(2) == -1 );
        assert( get_id_from_cpu(3) == -1 );
        assert( get_id_from_cpu(4) == 2 );
        assert( get_id_from_cpu(5) == 3 );
        assert( get_id_from_cpu(6) == -1 );
        assert( get_id_from_cpu(7) == -1 );
    }

    /* Test the task_create callback. Creates 4 tasks, ensure that they
       are counted, and check that no CPU is acquired. */
    {
        pending_tasks = 0;
        
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(pending_tasks == 1);
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(pending_tasks == 2);
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(pending_tasks == 3);
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(pending_tasks == 4);

        assert( get_id_from_cpu(0) == 0 );
        assert( get_id_from_cpu(1) == 1 );
        assert( get_id_from_cpu(2) == -1 );
        assert( get_id_from_cpu(3) == -1 );
        assert( get_id_from_cpu(4) == 2 );
        assert( get_id_from_cpu(5) == 3 );
        assert( get_id_from_cpu(6) == -1 );
        assert( get_id_from_cpu(7) == -1 );

        pending_tasks = 0;
    }

    /* Check the parallel_begin and parallel_end callbacks. First, check that the
       variables are updated correctly without nesting, and then under nested parallelism. */
    {
        ompt_frame_t encountering_task_frame = {.exit_frame.ptr = NULL };
        ompt_data_t parallel_data;
        omptm_role_shift__parallel_begin(NULL, &encountering_task_frame,
                &parallel_data, 4, ompt_parallel_team, NULL);
        assert(in_parallel);
        assert(current_parallel_size == 4);
        assert(parallel_data.value == PARALLEL_LEVEL_1);

        omptm_role_shift__parallel_end(&parallel_data, NULL, 0, NULL);
        assert(!in_parallel);
        assert(parallel_data.value == PARALLEL_UNSET);

        //Testing a nested parallel
        ompt_frame_t encountering_task_frame_level2 = {.exit_frame.ptr = &main};
        ompt_data_t parallel_data_level2;
        parallel_data_level2.value = 0;
        parallel_data_level2.ptr = 0x0;
        omptm_role_shift__parallel_begin(NULL, &encountering_task_frame,
                &parallel_data, 4, ompt_parallel_team, NULL);
        assert(in_parallel);
        assert(current_parallel_size == 4);
        assert(parallel_data.value == PARALLEL_LEVEL_1);

        omptm_role_shift__parallel_begin(NULL, &encountering_task_frame_level2,
                &parallel_data_level2, 2, ompt_parallel_team, NULL);
        assert(in_parallel);
        assert(parallel_data.value == PARALLEL_LEVEL_1);

        omptm_role_shift__parallel_end(&parallel_data_level2, NULL, 0, NULL);
        assert(in_parallel);
        assert(parallel_data.value == PARALLEL_LEVEL_1);

        omptm_role_shift__parallel_end(&parallel_data, NULL, 0, NULL);
        assert(!in_parallel);
        assert(parallel_data.value == PARALLEL_UNSET);
    }

    //Check basic actions of thread_role_shift callback
    {
        global_tid = 2;
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);

        //FA -> NONE transition, nothing should happen
        omptm_role_shift__thread_role_shift(NULL, 
                OMP_ROLE_FREE_AGENT, OMP_ROLE_NONE);
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);
        
        //NONE -> FA transition. Nothing should happen because the process owns the CPU
        pending_tasks = 0;
        omptm_role_shift__thread_role_shift(NULL,
                OMP_ROLE_NONE, OMP_ROLE_FREE_AGENT);
        assert(cpu_by_id[global_tid] == 4);
        assert(cpu_data[4].ownership == OWN);
    }

    // Enable MPI OMPT policy flag
    omptool_opts = OMPTOOL_OPTS_MPI;

    /* Test IntoBlockingCall and OutOfBlockingCall */
    {
        /* P1 is outside a parallel region, and invokes IntoBlockingCall */
        global_tid = 0;
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
                    cpus_priority_array, PRIO_ANY, 0,
                    &last_borrow, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid
                && new_guests[4] == p2_pid && new_guests[5] == p2_pid );

        /* P2 returns P1's CPUs */
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p1_mask, new_guests) == DLB_SUCCESS );

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
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        pending_tasks = 0;
        
        //Create two tasks. Free agents should be activated in CPUs 2 and 3
        thread_id = 4;
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert(free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);

        thread_id = 5;
        omptm_role_shift__task_create(NULL, NULL, NULL, ompt_task_explicit, 0, NULL);
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == UNKNOWN);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);
        
        assert(pending_tasks == 2);
        pending_tasks = 10;
        //Schdeule 2 tasks. Two free agents should be created in CPUs 6 and 7

        thread_id = 6;
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(!free_agent_threads_status[7]);

        thread_id = 7;
        omptm_role_shift__task_schedule(NULL, ompt_task_switch, NULL);
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
        global_tid = 5;
        //Update CPU data manually since thread_begin is not really called for this thread
        cpu_data[cpu_by_id[4]].fa = true;
        cpu_data[cpu_by_id[5]].fa = true;
        cpu_data[cpu_by_id[6]].fa = true;
        cpu_data[cpu_by_id[7]].fa = true;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == BORROWED);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        pending_tasks = 0;
        //A task is finished an there are no pending tasks. 
        //The CPU should be lend to DLB and the state updated
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        assert(cpu_data[2].ownership == BORROWED);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        //P2 reclaims its process mask. CPU 3 can be immediately reclaimed
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid &&
                new_guests[6] == p2_pid && new_guests[7] == p2_pid );
        assert( victims[2] == p1_pid && victims[3] == -1 &&
                victims[6] == p1_pid && victims[7] == p1_pid );

        //Threads 4, 6, and 7 (Free agents) finish a task.
        //The CPUs should be disabled and the threads change their role
        global_tid = 4;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == BORROWED);
        assert(cpu_data[7].ownership == BORROWED);
        assert(!free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        global_tid = 6;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
        assert(cpu_data[2].ownership == UNKNOWN);
        assert(cpu_data[3].ownership == UNKNOWN);
        assert(cpu_data[6].ownership == UNKNOWN);
        assert(cpu_data[7].ownership == BORROWED);
        assert(!free_agent_threads_status[4]);
        assert(!free_agent_threads_status[5]);
        assert(!free_agent_threads_status[6]);
        assert(free_agent_threads_status[7]);

        global_tid = 7;
        omptm_role_shift__task_schedule(NULL, ompt_task_complete, NULL);
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
    assert( shmem_cpuinfo__finalize(p2_pid, thread_spd->options.shm_key) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, thread_spd->options.shm_key) == DLB_SUCCESS );

    return 0;
}
