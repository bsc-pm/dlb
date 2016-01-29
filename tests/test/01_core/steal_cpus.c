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
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/sighandler.h"
#include "support/mask_utils.h"

static int num_cpus;

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

        i=0;
        while(i<100) {
            shmem_procinfo__update(true, true);
            const struct timespec delay = {0, 10000000L};
            nanosleep(&delay, NULL);
            i++;
        }

        printf("Child process finishing\n");

        // Unload DLB modules
        shmem_cpuinfo__finalize();
        shmem_procinfo__finalize();
        unregister_signals();
        options_finalize();
        _exit(EXIT_SUCCESS);

    } else if (child_pid > 0) {
        // Parent process
        return child_pid;
    } else {
        fprintf(stderr, "Fork failed\n");
    }
    return -1;

}

static void do_test(int num_childs) {
    pid_t childs[num_childs];
    int i;
    for (i=0; i<num_childs; ++i) {
        printf("Parent doing child %d\n", i);
        childs[i] = do_child(i, num_childs);
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

    int status;
    for (i=0; i<num_childs; ++i) {
        waitpid(childs[i], &status, 0);
    }
}

int main( int argc, char **argv ) {

    num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    do_test(2);
    return 0;
}
