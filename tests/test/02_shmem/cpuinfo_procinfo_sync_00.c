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
</testinfo>*/

#include "assert_noshm.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/error.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

// Test synchronization between procinfo and cpuinfo when the ownership changes

/* static cpu_set_t process_mask; */

static void* thread_start(void *arg) {
    pid_t pid = *(pid_t*)arg;

    // Initialize external
    assert( shmem_procinfo_ext__init(NULL) == DLB_SUCCESS );

    // External sets new mask
    cpu_set_t new_mask = { .__bits = {0x3} }; // [01--]
    assert( shmem_procinfo__setprocessmask(pid, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );

    //Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    return NULL;
}

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    pid_t pid = 42;
    cpu_set_t mask = { .__bits = {0xf} }; // [0123]

    // Initialize sub-process
    assert( shmem_procinfo__init(pid, &mask, NULL, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(pid, &mask, NULL) == DLB_SUCCESS );

    // Get process mask

    // Create a new thread to simulate an external process and set a new mask
    {
        pthread_t thread;
        pthread_create(&thread, NULL, thread_start, (void*)&pid);

        // Poll until thread sets a new mask
        while (shmem_procinfo__polldrom(pid, NULL, &mask) != DLB_SUCCESS) {
            usleep(1000);
        }

        // Once we've obtained the new mask, the thread can die
        pthread_join(thread, NULL);
    }

    // Update cpuinfo with the new mask
    shmem_cpuinfo__update_ownership(pid, &mask);

    // cpuinfo should be dirty now
    assert( shmem_cpuinfo__is_dirty() );

    // Get new bindings for thread 0
    assert( shmem_cpuinfo__get_thread_binding(pid, 0) == 0 );

    // cpuinfo should still be dirty
    assert( shmem_cpuinfo__is_dirty() );

    // Get new bindings for thread 1
    assert( shmem_cpuinfo__get_thread_binding(pid, 1) == 1 );

    // cpuinfo should not be dirty
    assert( !shmem_cpuinfo__is_dirty() );

    // Finalize sub-process
    assert( shmem_cpuinfo__finalize(pid) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(pid, false) == DLB_SUCCESS );

    return 0;
}
