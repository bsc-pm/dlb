/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 1 sub-process

int main( int argc, char **argv ) {
    pid_t pid = getpid();
    cpu_set_t process_mask, mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    int mycpu = sched_getcpu();
    printf("mycpu: %d\n", mycpu);
    int system_size = mu_get_system_size();
    pid_t new_guests[system_size];
    pid_t victims[system_size];
    int cpus_priority_array[system_size];
    int64_t last_borrow = 0;
    int i, j;

    // Setup dummy priority CPUs
    for (i=0, j=0; i<system_size; ++i) {
        if (CPU_ISSET(i, &process_mask)) {
            cpus_priority_array[j++] = i;
        }
    }
    for  (;j<system_size; ++j) {
        cpus_priority_array[j] = -1;
    }

    // Init
    assert( shmem_cpuinfo__init(pid, &process_mask, NULL) == DLB_SUCCESS );

    /* Tests using mycpu */

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Reclaim CPU
    assert( shmem_cpuinfo__reclaim_cpu(pid, mycpu, &new_guests[0], &victims[0]) == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    assert( victims[0] == -1 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Reclaim CPUs
    assert( shmem_cpuinfo__reclaim_cpus(pid, 1, new_guests, victims) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( i==mycpu ? new_guests[i] == pid : new_guests[i] <=0 );
        assert( victims[i] == -1 );
    }

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Acquire CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, mycpu, &new_guests[0], &victims[0]) == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    assert( victims[0] == -1 );
    assert( shmem_cpuinfo__acquire_cpu(pid, mycpu, &new_guests[0], &victims[0]) == DLB_NOUPDT );
    assert( new_guests[0] == -1 );
    assert( victims[0] == -1 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, mycpu, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Borrow CPUs
    assert( shmem_cpuinfo__borrow_cpus(pid, PRIO_ANY, cpus_priority_array, &last_borrow,
                1, new_guests)
            == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( i==mycpu ? new_guests[i] == pid : new_guests[i] == -1 );
    }

    /* Tests using masks */

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == 0 : new_guests[i] == -1 );
    }

    // Reclaim mask
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &process_mask, new_guests, victims) >= 0 );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == pid : new_guests[i] == -1 );
        assert( victims[i] == -1 );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == 0 : new_guests[i] == -1 );
    }

    // Reclaim all
    assert( shmem_cpuinfo__reclaim_all(pid, new_guests, victims) >= 0 );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == pid : new_guests[i] == -1 );
        assert( victims[i] == -1 );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == 0 : new_guests[i] == -1 );
    }

    // Acquire mask
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &process_mask, new_guests, victims) >= 0 );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == pid : new_guests[i] == -1 );
        assert( victims[i] == -1 );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == 0 : new_guests[i] == -1 );
    }

    // Borrow all
    assert( shmem_cpuinfo__borrow_all(pid, PRIO_ANY, cpus_priority_array, &last_borrow,
                new_guests) >= 0 );
    for (i=0; i<system_size; ++i) {
        assert( CPU_ISSET(i, &process_mask) ? new_guests[i] == pid : new_guests[i] == -1 );
    }

    // Return
    assert( shmem_cpuinfo__return_all(pid, new_guests) == DLB_NOUPDT );
    for (i=0; i<system_size; ++i) { assert( new_guests[i] == -1 ); }
    assert( shmem_cpuinfo__return_cpu(pid, mycpu, &new_guests[0]) == DLB_NOUPDT );
    assert( new_guests[0] == -1 );
    assert( shmem_cpuinfo__return_cpu_mask(pid, &process_mask, new_guests) == DLB_NOUPDT );
    for (i=0; i<system_size; ++i) { assert( new_guests[i] == -1 ); }

    // MaxParallelism
    int max;
    for (max=1; max<system_size; ++max) {
        // Set up 'max' CPUs
        assert( shmem_cpuinfo__update_max_parallelism(pid, max, new_guests, victims)
                == DLB_SUCCESS );
        for (i=0; i<max; ++i) { assert( new_guests[i] == -1 && victims[i] == -1 ); }
        for (i=max; i<system_size; ++i) { assert( new_guests[i] == 0  && victims[i] == pid ); }
        // Acquire all CPUs again
        assert( shmem_cpuinfo__acquire_cpu_mask(pid, &process_mask, new_guests, victims)
                == DLB_SUCCESS  );
    }

    // Check errors with a nonexistent CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, system_size, &new_guests[0], &victims[0])
            == DLB_ERR_PERM );
    assert( shmem_cpuinfo__reclaim_cpu(pid, system_size, &new_guests[0], &victims[0])
            == DLB_ERR_PERM );
    CPU_ZERO(&mask);
    CPU_SET(system_size, &mask);
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &mask, new_guests, victims) == DLB_SUCCESS );
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &mask, new_guests, victims) == DLB_NOUPDT );

    // Deregister process and check that no action is needed
    assert( shmem_cpuinfo__deregister(pid, new_guests, victims) == DLB_SUCCESS );
    for (i=0; i<system_size; ++i) { assert( new_guests[i] == -1 ); }
    for (i=0; i<system_size; ++i) { assert( victims[i] == -1 ); }

    // Finalize
    assert( shmem_cpuinfo__finalize(pid, NULL) == DLB_SUCCESS );

    return 0;
}
