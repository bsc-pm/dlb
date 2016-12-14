/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_LDFLAGS=-pthread
    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/sighandler.h"
#include "support/mask_utils.h"

#define NUM_SEMS 3
#define MAX_CHILDS 2
static sem_t *sem[MAX_CHILDS][NUM_SEMS];
static int num_cpus;

// This function simulates a process to be allocated in the cpuset that has been stolen
static void launch_intruder(const cpu_set_t *mask) {
    sched_setaffinity(0, sizeof(cpu_set_t), mask);

    // Load basic DLB modules
    options_init();
    pm_init();
    debug_init();
    register_signals();
    shmem_procinfo__init();
    shmem_cpuinfo__init();

    // Unload DLB modules
    shmem_cpuinfo__finalize();
    shmem_procinfo__finalize();
    unregister_signals();
    options_finalize();
}

static pid_t do_child(int id, int num_childs) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process

        // Compute and set new mask
        int cpus_per_child = num_cpus / num_childs;
        int cpu_start = id*cpus_per_child;
        int cpu_end = (id+1)*cpus_per_child;
        cpu_set_t new_mask;
        CPU_ZERO(&new_mask);
        int i;
        for (i=cpu_start; i<cpu_end; ++i) {
            CPU_SET(i, &new_mask);
        }
        sched_setaffinity(0, sizeof(cpu_set_t), &new_mask);
        printf("Child running with mask: %s\n", mu_to_str(&new_mask));

        // Load basic DLB modules
        setenv("LB_VERBOSE", "shmem", 1);
        pm_init();
        options_init();
        debug_init();
        register_signals();
        shmem_procinfo__init();
        shmem_cpuinfo__init();

        // Save our initial mask
        cpu_set_t initial_mask;
        shmem_procinfo__getprocessmask(getpid(), &initial_mask);

        // Wait until parent process change our mask
        sem_post(sem[id][0]);
        sem_wait(sem[id][1]);

        // Update process mask and signal parent
        shmem_procinfo__update(true, true);
        shmem_procinfo__getprocessmask(getpid(), &new_mask);
        shmem_cpuinfo__update_ownership(&new_mask);
        printf("Child running with mask: %s\n", mu_to_str(&new_mask));
        sem_post(sem[id][2]);

        // Wait until an intruder process gets and then leaves our CPUs
        sem_wait(sem[id][0]);
        shmem_procinfo__update(true, true);
        shmem_procinfo__getprocessmask(getpid(), &new_mask);
        shmem_cpuinfo__update_ownership(&new_mask);
        printf("Child running with mask: %s\n", mu_to_str(&new_mask));

        // Compare initial and current mask (they should be the same)
        int error = CPU_EQUAL(&initial_mask, &new_mask) ? EXIT_SUCCESS : EXIT_FAILURE;

        printf("Child process finishing\n");

        // Unload DLB modules
        shmem_cpuinfo__finalize();
        shmem_procinfo__finalize();
        unregister_signals();
        options_finalize();

        for (i=0; i<NUM_SEMS; i++) {
            munmap(sem[i], sizeof(sem_t));
        }
        _exit(error);

    } else if (child_pid > 0) {
        // Parent process
        return child_pid;
    } else {
        fprintf(stderr, "Fork failed\n");
    }
    return EXIT_FAILURE;

}

static int do_test(int num_childs) {
    int error = EXIT_SUCCESS;
    pid_t childs[num_childs];
    int i;
    for (i=0; i<num_childs; ++i) {

        /* Create semaphores */
        int j;
        for (j=0; j<NUM_SEMS; j++) {
            sem[i][j] = mmap(NULL, sizeof(sem_t),
                PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
            if (sem[i][j] == MAP_FAILED) {
                perror("mmap");
                exit(EXIT_FAILURE);
            }
            if (sem_init(sem[i][j], 1, 0) < 0 ) {
                perror("sem_init");
                exit(EXIT_FAILURE);
            }
        }

        /* Do childs */
        printf("Parent doing child %d\n", i);
        childs[i] = do_child(i, num_childs);
    }

    // Wait for all childs to be initialized
    printf("Parent waiting...\n");
    for (i=0; i<num_childs; ++i) {
        sem_wait(sem[i][0]);
    }

    int ncpus = 2;
    int steal = 1;
    int max_len = 2;
    int cpulist[max_len];
    int nelems;
    setenv("LB_VERBOSE", "shmem", 1);
    pm_init();
    options_init();
    debug_init();
    shmem_procinfo_ext__init();
    printf("Stealing CPUs\n");
    shmem_procinfo_ext__getcpus(ncpus, steal, cpulist, &nelems, max_len);
    printf("CPUs stolen: %d\n", nelems);
    shmem_procinfo_ext__finalize();

    if (nelems != ncpus) {
        exit(EXIT_FAILURE);
    }

    // Signal all childs to let them update their mask
    for (i=0; i<num_childs; ++i) {
        sem_post(sem[i][1]);
    }

    // Wait until all of them have updated it
    for (i=0; i<num_childs; ++i) {
        sem_wait(sem[i][2]);
    }

    // simulate a new process running on the stolen CPUs
    cpu_set_t intruder_mask;
    CPU_ZERO(&intruder_mask);
    for (i=0; i<nelems; i++) {
        CPU_SET(cpulist[i], &intruder_mask);
    }
    launch_intruder(&intruder_mask);

    // Signal all childs again so they can recover their CPUs
    for (i=0; i<num_childs; ++i) {
        sem_post(sem[i][0]);
    }

    int status;
    for (i=0; i<num_childs; ++i) {
        int j;
        for (j=0; j<NUM_SEMS; j++) {
            sem_destroy(sem[i][j]);
            munmap(sem[i][j], sizeof(sem_t));
        }
        waitpid(childs[i], &status, 0);
        error += status;
    }

    error += nelems - ncpus;

    return error;
}

int main( int argc, char **argv ) {
    int error = EXIT_SUCCESS;
    num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    // This test creates 2 processes and asks for 2 CPUs, so a minimium of 4 CPUs is required
    if (num_cpus >= 4) {
        error += do_test(2);
    }

    return error;
}
