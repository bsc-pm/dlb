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
    test_generator="gens/basic-generator -a --mode=polling|--mode=async"
</testinfo>*/

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/options.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Basic checks for checking Borrow and Acquire features

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

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

    // Initialize options and enable queues if needed
    options_t options;
    options_init(&options, NULL);
    bool async = options.mode == MODE_ASYNC;
    if (async) {
        shmem_cpuinfo__enable_request_queues();
    }
    int64_t last_borrow_value = 0;
    int64_t *last_borrow = async ? NULL : &last_borrow_value;

    // Setup dummy priority CPUs
    int cpus_priority_array[SYS_SIZE];
    int i;
    for (i=0; i<SYS_SIZE; ++i) cpus_priority_array[i] = i;

    int err;

    /*** BorrowCpus test ***/
    {
        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] <= 0 );

        // Process 1 wants to BORROW up to 2 CPUs
        assert( shmem_cpuinfo__borrow_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, 2, new_guests) == DLB_SUCCESS );
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
        err = shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, 2, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );
        assert( new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 2 releases CPU 2
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 2, &new_guests[0]) == DLB_SUCCESS );
        assert( async ? new_guests[0] == p1_pid : new_guests[0] == 0 );

        // If polling, process 1 needs to ask again for the last CPU
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                        last_borrow, 1, new_guests, victims) == DLB_SUCCESS );
            assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[3] == -1 );
            assert( new_guests[2] == p1_pid );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

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
        err = shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, 2, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_SUCCESS );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );
        assert( new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 1 wants to cancel previous request
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, NULL, last_borrow, 0,
                    new_guests, victims) == DLB_SUCCESS );
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

    /*** AcquireCpus one by one with some CPUs already reclaimed ***/
    {
        // Process 1 releases CPUs 0 and 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );

        // Process 2 acquires CPU 0
        assert( shmem_cpuinfo__acquire_cpu(p2_pid, 0, &new_guests[0], &victims[0])
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
        assert( victims[0] == -1 );

        // Process 1 acquires 1 CPU (reclaims CPU 0)
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, 1, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == p1_pid );
        assert( new_guests[1] == -1 && new_guests[2] == -1 && new_guests[3] == -1 );
        assert( victims[0] == p2_pid );
        assert( victims[1] == -1 && victims[2] == -1 && victims[3] == -1 );

        // Process 1 acquires 1 CPU (acquires CPU 1 currently idle)
        assert( shmem_cpuinfo__acquire_cpus(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, 1, new_guests, victims) == DLB_SUCCESS );
        assert( new_guests[1] == p1_pid );
        assert( new_guests[0] == -1 && new_guests[2] == -1 && new_guests[3] == -1 );
        assert( victims[0] == -1 && victims[1] == -1 && victims[2] == -1 && victims[3] == -1 );

        // Process 2 returns CPU 0
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 0, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p1_pid );
    }

    /*** AcquireCpus of CPUs not in the priority array ***/
    {
        // Process 2 acquires 1 CPU, but only allowing CPUs {1-3}
        for (i=0; i<SYS_SIZE; ++i) cpus_priority_array[i] = i+1 < SYS_SIZE ? i+1 : -1;
        err = shmem_cpuinfo__acquire_cpus(p2_pid, PRIO_ANY, cpus_priority_array,
                last_borrow, 1, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 1 releases CPU 0
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );

        // If polling, process 2 needs to ask again
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpus(p2_pid, PRIO_ANY, cpus_priority_array,
                        last_borrow, 1, new_guests, victims) == DLB_NOUPDT );
            for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // Process 1 acquires CPU 0 (currently idle)
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 0, &new_guests[0], &victims[0])
                == DLB_SUCCESS );
        assert( new_guests[0] == p1_pid );
        assert( victims[0] == -1 );

        // Process 1 releases CPU 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &new_guests[0]) == DLB_SUCCESS );
        assert( async ? new_guests[0] == p2_pid : new_guests[0] == 0 );

        // If polling, process 2 needs to ask again
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpus(p2_pid, PRIO_ANY, cpus_priority_array,
                        last_borrow, 1, new_guests, victims) == DLB_SUCCESS );
            assert( new_guests[1] == p2_pid );
            assert( new_guests[0] == -1 && new_guests[2] == -1 && new_guests[3] == -1 );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // Process 2 releases CPU 1
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 1, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );

        // 1 acquires CPU 1 (currently idle)
        assert( shmem_cpuinfo__acquire_cpu(p1_pid, 1, &new_guests[0], &victims[0])
                == DLB_SUCCESS );
        assert( new_guests[0] == p1_pid );
        assert( victims[0] == -1 );
    }

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, NULL) == DLB_SUCCESS );

    /* Test lend post mortem feature */
    {
        // Reset cpu priority
        for (i=0; i<SYS_SIZE; ++i) cpus_priority_array[i] = i;

        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P1 finalizes
        assert( shmem_cpuinfo__finalize(p1_pid, NULL) == DLB_SUCCESS );

        // P2 borrows P1 CPUs
        assert( shmem_cpuinfo__borrow_all(p2_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid );
        assert( new_guests[2] == -1 && new_guests[3] == -1 );

        // P2 finalizes
        assert( shmem_cpuinfo__finalize(p2_pid, NULL) == DLB_SUCCESS );
    }

    /* Test early finalization with pending actions */
    {
        // Reset cpu priority
        for (i=0; i<SYS_SIZE; ++i) cpus_priority_array[i] = i;

        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P2 lends CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // P1 borrows all
        assert( shmem_cpuinfo__borrow_all(p1_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );

        // P2 asks for as many CPUs as SYS_SIZE
        err = shmem_cpuinfo__acquire_cpus(p2_pid, PRIO_ANY, cpus_priority_array,
                    last_borrow, SYS_SIZE, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // P1 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, new_guests, victims) == DLB_SUCCESS );
        if (async) {
            for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == p2_pid ); }
        } else {
            for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == 0 ); }
        }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__finalize(p1_pid, NULL) == DLB_SUCCESS );

        // If polling, P2 needs to ask again for SYS_SIZE CPUs
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpus(p2_pid, PRIO_ANY, cpus_priority_array,
                        last_borrow, SYS_SIZE, new_guests, victims) == DLB_SUCCESS );
            for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == p2_pid ); }
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // P2 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, new_guests, victims) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__finalize(p2_pid, NULL) == DLB_SUCCESS );
    }

    return 0;
}
