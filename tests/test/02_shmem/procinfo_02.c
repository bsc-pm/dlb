/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#include "assert_noshm.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/error.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
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

int main( int argc, char **argv ) {
    pid_t pid = getpid();
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    char shm_filename[SHM_NAME_LENGTH+8];
    snprintf(shm_filename, SHM_NAME_LENGTH+8, "/dev/shm/DLB_procinfo_%d", getuid());

    // Initialize sub-process
    assert( shmem_procinfo__init(pid, &process_mask, NULL, NULL) == DLB_SUCCESS );

    // Initialize external
    assert( shmem_procinfo_ext__init(NULL) == DLB_SUCCESS );

    // Get pidlist
    int pidlist[num_cpus];
    int nelems = 0;
    shmem_procinfo_ext__getpidlist(pidlist, &nelems, num_cpus);
    assert( nelems == 1 );
    assert( pidlist[0] == pid );

    // Get process mask
    assert( shmem_procinfo_ext__getprocessmask(pid, &mask) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &mask) );

    // Create a new thread to poll while the external sub-process sets a new mask
    {
        pthread_t thread;
        pthread_create(&thread, NULL, thread_start, (void*)&pid);

        // Set a new process mask (cpuid=0 to comply with single core machines)
        CPU_CLR(0, &mask);
        assert( shmem_procinfo_ext__setprocessmask(pid, &mask) == DLB_SUCCESS );

        pthread_join(thread, NULL);

        // Check that masks are equivalent
        assert( shmem_procinfo_ext__getprocessmask(pid, &mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&process_mask, &mask) );
    }

    //Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    // Finalize sub-process
    assert( shmem_procinfo__finalize(pid) == DLB_SUCCESS );

    return 0;
}
