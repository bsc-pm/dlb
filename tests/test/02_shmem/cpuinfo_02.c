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

#include "unique_shmem.h"

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
#include <string.h>
#include <assert.h>

// Basic checks with 2 sub-processes with masks

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
    int i;

    // Init
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p1_mask, SHMEM_KEY) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );

    // Initialize options and enable queues if needed
    options_t options;
    options_init(&options, NULL);
    bool async = options.mode == MODE_ASYNC;
    if (async) {
        shmem_cpuinfo__enable_request_queues();
    }

    int err;

    /*** Successful ping-pong ***/
    {
        // Process 1 wants CPUs 2 & 3
        err = shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        assert(          new_guests[0] == -1     && new_guests[1] == -1 );
        assert(  async ? new_guests[2] == p1_pid && new_guests[3] == p1_pid :
                         new_guests[2] == 0      && new_guests[3] == 0 );

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                    == DLB_SUCCESS );
            assert( new_guests[0] == -1     && new_guests[1] == -1 );
            assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // Process 1 cannot reclaim CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                == DLB_ERR_PERM );

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, new_guests, victims)
                == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );

        // Process 1 returns CPUs 2 & 3
        assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );

        // Process 2 releases CPUs 2 & 3 again
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        assert(         new_guests[0] == -1     && new_guests[1] == -1 );
        assert( async ? new_guests[2] == p1_pid && new_guests[3] == p1_pid :
                        new_guests[2] == 0      && new_guests[3] == 0 );

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                    == DLB_SUCCESS );
            assert( new_guests[0] == -1     && new_guests[1] == -1 );
            assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // Process 2 reclaims CPUs 2 & 3 again
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, new_guests, victims)
                == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );

        // Process 1 returns CPUs 2 & 3 and removes petition
        assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }

        // Process 2 releases CPUs 2 & 3, checks no victim and reclaims
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
    }

    /*** Successful ping-pong, but using reclaim_all and return_all ***/
    {
        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // Process 1 wants CPUs 2 & 3
        assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 2 reclaims all
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );

        // Process 1 returns all
        assert( shmem_cpuinfo__return_all(p1_pid, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );

        // Process 2 releases CPUs 2 & 3 again
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        assert(         new_guests[0] == -1     && new_guests[1] == -1 );
        assert( async ? new_guests[2] == p1_pid && new_guests[3] == p1_pid :
                        new_guests[2] == 0      && new_guests[3] == 0 );

        // If polling, process 1 needs to ask again for CPUs 2 & 3
        if (!async) {
            assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                    == DLB_SUCCESS );
            assert( new_guests[0] == -1     && new_guests[1] == -1 );
            assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // Process 2 reclaims all again
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );

        // Process 1 returns all and removes petition
        assert( shmem_cpuinfo__return_all(p1_pid, new_guests) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // Process 2 releases CPUs 2 & 3, checks no new guests and reclaims
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }
        assert( shmem_cpuinfo__reclaim_all(p2_pid, new_guests, victims) == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
    }

    /*** Late reply ***/
    {
        // Process 1 wants CPUs 2 & 3
        err = shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // Process 1 no longer wants CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
    }

    /*** MaxParallelism ***/
    {
        // Process 1 lends CPU 0
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 0, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );

        cpu_set_t mask;
        mu_parse_mask("0,2-3", &mask);

        // TEST 1: process 2 is guesting 0,2-3 and sets max_parellelism to 1
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
        for (i=1; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 1, new_guests, victims)
                 == DLB_SUCCESS );
        assert( new_guests[0] == 0  && victims[0] == p2_pid );
        assert( new_guests[1] == -1 && victims[1] == -1 );
        assert( new_guests[2] == -1 && victims[2] == -1 );
        assert( new_guests[3] == 0  && victims[3] == p2_pid );

        // TEST 2: process 2 is guesting 0,2-3 and sets max_parellelism to 2
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid && new_guests[3] == p2_pid);
        assert( new_guests[1] == -1     && new_guests[2] == -1 );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 2, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == 0  && victims[0] == p2_pid );
        assert( new_guests[1] == -1 && victims[1] == -1 );
        assert( new_guests[2] == -1 && victims[2] == -1 );
        assert( new_guests[3] == -1 && victims[3] == -1 );

        // TEST 3: process 2 is guesting 0,2-3 and sets max_parellelism to 3
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid );
        for (i=1; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 3, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1 && victims[0] == -1 );
        assert( new_guests[1] == -1 && victims[1] == -1 );
        assert( new_guests[2] == -1 && victims[2] == -1 );
        assert( new_guests[3] == -1 && victims[3] == -1 );

        // Process 1 lends CPU 1
        assert( shmem_cpuinfo__lend_cpu(p1_pid, 1, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == 0 );

        mu_parse_mask("0-3", &mask);

        // TEST 4: process 2 is guesting 0-3 and sets max_parellelism to 1
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[1] == p2_pid );
        assert( new_guests[0] == -1 && new_guests[2] == -1 && new_guests[3] == -1 );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 1, new_guests, victims)
                 == DLB_SUCCESS );
        assert( new_guests[0] == 0  && victims[0] == p2_pid );
        assert( new_guests[1] == 0  && victims[1] == p2_pid );
        assert( new_guests[2] == -1 && victims[2] == -1 );
        assert( new_guests[3] == 0  && victims[3] == p2_pid );

        // TEST 5: process 2 is guesting 0-3 and sets max_parellelism to 2
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid  && new_guests[1] == p2_pid && new_guests[3] == p2_pid );
        assert( new_guests[2] == -1 );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 2, new_guests, victims)
                 == DLB_SUCCESS );
        assert( new_guests[0] == 0  && victims[0] == p2_pid );
        assert( new_guests[1] == 0  && victims[1] == p2_pid );
        assert( new_guests[2] == -1 && victims[2] == -1 );
        assert( new_guests[3] == -1 && victims[3] == -1 );

        // TEST 6: process 2 is guesting 0-3 and sets max_parellelism to 3, P1 reclaims
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid );
        assert( new_guests[2] == -1     && new_guests[3] == -1     );
        assert( shmem_cpuinfo__reclaim_all(p1_pid, new_guests, victims) == DLB_NOTED );
        assert( new_guests[0] == p1_pid && new_guests[1] == p1_pid );
        assert( new_guests[2] == -1     && new_guests[3] == -1 );
        assert( victims[0] == p2_pid && victims[1] == p2_pid );
        assert( victims[2] == -1     && victims[3] == -1 );
        assert( shmem_cpuinfo__update_max_parallelism(p2_pid, 3, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1     && victims[0] == -1 );
        assert( new_guests[1] == p1_pid && victims[1] == p2_pid );
        assert( new_guests[2] == -1     && victims[2] == -1 );
        assert( new_guests[3] == -1     && victims[3] == -1 );

        // Process 2 lends CPU 0
        assert( shmem_cpuinfo__lend_cpu(p2_pid, 0, &new_guests[0]) == DLB_SUCCESS );
        assert( new_guests[0] == p1_pid );
    }

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );

    /* Test lend post mortem feature */
    {
        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P1 finalizes
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );

        // P2 borrows P1 CPUs
        assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &p1_mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid );
        assert( new_guests[2] == -1 && new_guests[3] == -1 );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // P2 finalizes
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );
    }

    /* Test early finalization with pending actions */
    {
        // Set up fake spd to set post-mortem option
        subprocess_descriptor_t spd;
        spd.options.debug_opts = DBG_LPOSTMORTEM;
        spd_enter_dlb(&spd);

        // Initialize
        assert( shmem_cpuinfo__init(p1_pid, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );
        if (async) { shmem_cpuinfo__enable_request_queues(); }

        // P2 lends CPUs 2 & 3
        assert( shmem_cpuinfo__lend_cpu_mask(p2_pid, &p2_mask, new_guests) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] <= 0 ); }

        // P1 acquires CPU 2 & 3
        assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, new_guests, victims)
                == DLB_SUCCESS );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p1_pid && new_guests[3] == p1_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }

        // P2 reclaims and requests everything
        assert( shmem_cpuinfo__reclaim_cpu_mask(p2_pid, &p2_mask, new_guests, victims)
                == DLB_NOTED );
        assert( new_guests[0] == -1     && new_guests[1] == -1 );
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        assert( victims[0] == -1     && victims[1] == -1 );
        assert( victims[2] == p1_pid && victims[3] == p1_pid );
        err = shmem_cpuinfo__acquire_cpu_mask(p2_pid, &p1_mask, new_guests, victims);
        assert( async ? err == DLB_NOTED : err == DLB_NOUPDT );

        // P1 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, new_guests, victims) == DLB_SUCCESS );
        if (async) {
            assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid );
        } else {
            assert( new_guests[0] == 0 && new_guests[1] == 0 );
        }
        assert( new_guests[2] == p2_pid && new_guests[3] == p2_pid );
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );

        // If polling, P2 needs to ask again for CPUs 0-3
        if (!async) {
            assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 2) == DLB_SUCCESS );
            assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );
            assert( shmem_cpuinfo__acquire_cpu_mask(p2_pid, &p1_mask, new_guests, victims)
                    == DLB_SUCCESS );
            assert( new_guests[0] == p2_pid && new_guests[1] == p2_pid );
            assert( new_guests[2] == -1     && new_guests[3] == -1 );
            for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        }

        // P2 finalizes
        assert( shmem_cpuinfo__deregister(p1_pid, new_guests, victims) == DLB_SUCCESS );
        for (i=0; i<SYS_SIZE; ++i) { assert( new_guests[i] == -1 ); }
        for (i=0; i<SYS_SIZE; ++i) { assert( victims[i] == -1 ); }
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );
    }

    return 0;
}
