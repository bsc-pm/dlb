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
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Basic checks for checking Borrow and Acquire features

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum {SYS_SIZE = 4};
    mu_init();
    int ncpus = mu_get_system_size();
    if (ncpus < SYS_SIZE) {
        ncpus = SYS_SIZE;
        mu_testing_set_sys_size(SYS_SIZE);
    }

    // Initialize local masks to [1100] and [0011]
    pid_t p1_pid = 111;
    cpu_set_t p1_mask;
    CPU_ZERO(&p1_mask);
    CPU_SET(0, &p1_mask);
    CPU_SET(1, &p1_mask);
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    CPU_ZERO(&p2_mask);
    CPU_SET(2, &p2_mask);
    CPU_SET(3, &p2_mask);
    pid_t new_guests[SYS_SIZE];
    pid_t victims[SYS_SIZE];

    // Init
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p1_mask, NULL) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );

    // Setup dummy priority CPUs
    int cpus_priority_array[ncpus];
    int i;
    for (i=0; i<ncpus; ++i) cpus_priority_array[i] = i;

    /*** BorrowCpus test ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 1 wants to BORROW up to 2 CPUs
        assert( shmem_cpuinfo__borrow_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    2, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1);
        assert( new_guests[3] == p1_pid );

        // Process 2 releases CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1 && victims[1] == -1 && victims[2] == -1 );
        assert( victims[3] == p1_pid );

        // Process 1 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
    }

    /*** AcquireCpus test ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 1 wants to ACQUIRE 2 CPUs
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    2, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );
        assert( new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 2 releases CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p1_pid );

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );

        // Process 1 releases CPU 2 and 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 2, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
    }

    /*** AcquireCpus with late reply ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 1 wants to ACQUIRE 2 CPUs
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    2, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );
        assert( new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 1 wants to cancel previous request
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, NULL, 0, new_guests, victims)
                == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 1 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_SUCCESS );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );
        assert( new_guests[3] == p2_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
    }

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid) == DLB_SUCCESS );

    return 0;
}
