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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/error.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

// Basic checks with 1 sub-process attached and 1 external

static cpu_set_t process_mask;

static void* thread_start(void *arg) {
    pid_t pid = *(pid_t*)arg;
    while (shmem_procinfo__polldrom(pid, NULL, &process_mask) != DLB_SUCCESS) {
        usleep(1000);
    }
    return NULL;
}

enum { MIN_CPUS = 2 };

int main( int argc, char **argv ) {
    int num_cpus = mu_get_system_size();
    if (num_cpus < MIN_CPUS) {
        mu_testing_set_sys_size(MIN_CPUS);
    }

    /* Current process mask */
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    /* Obtain a new_mask */
    cpu_set_t new_mask;
    CPU_ZERO(&new_mask);
    if (CPU_COUNT(&process_mask) > 1) {
        // Current process mask contains at least 2 CPUs, use the first one
        int i;
        for (i=0; i<num_cpus; ++i) {
            if (CPU_ISSET(i, &process_mask)) {
                CPU_SET(i, &new_mask);
                break;
            }
        }
    } else {
        // Current mask only contains 1 CPU, use a 'system mask'
        int i;
        for (i=0; i<num_cpus; ++i) {
            CPU_SET(i, &new_mask);
        }
    }

    // Initialize sub-process
    pid_t pid = getpid();
    assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );

    // Initialize external
    assert( shmem_procinfo_ext__init(SHMEM_KEY) == DLB_SUCCESS );

    // Get pidlist
    int *pidlist = malloc(sizeof(int) * num_cpus);
    int nelems = 0;
    shmem_procinfo__getpidlist(pidlist, &nelems, num_cpus);
    assert( nelems == 1 );
    assert( pidlist[0] == pid );

    // Get process mask
    cpu_set_t mask;
    assert( shmem_procinfo__getprocessmask(pid, &mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &mask) );

    // Create a new thread to poll while the external sub-process sets a new mask
    {
        pthread_t thread;
        pthread_create(&thread, NULL, thread_start, (void*)&pid);

        // Set a new process mask
        assert( shmem_procinfo__setprocessmask(pid, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );

        pthread_join(thread, NULL);

        // Check that masks are equivalent
        assert( shmem_procinfo__getprocessmask(pid, &mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
        assert( CPU_EQUAL(&new_mask, &mask) );
    }

    // Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    // Finalize sub-process
    assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_SUCCESS );

    free(pidlist);

    return 0;
}
