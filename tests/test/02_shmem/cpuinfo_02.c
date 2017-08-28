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
#include <string.h>
#include <assert.h>

// Basic checks with 2 sub-processes with masks

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    mu_init();
    if (mu_get_system_size() < 4) {
        mu_testing_set_sys_size(4);
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
    pid_t victimlist[4];

    // Init
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p1_mask, NULL) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );


    /*** Successful ping-pong ***/
    {
        // Process 1 wants CPUs 2 & 3
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_NOTED );
        assert( victimlist[2] == 0 && victimlist[3] == 0 );

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__recover_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_ERR_PERM );
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__recover_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_NOTED );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 1 returns CPUs 2 & 3
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p2_pid && victimlist[3] == p2_pid );

        // Process 2 releases CPUs 2 & 3 again
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 2 reclaims CPUs 2 & 3 again
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__recover_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_NOTED );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 1 returns CPUs 2 & 3 and removes petition
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__return_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p2_pid && victimlist[3] == p2_pid );
        assert( shmem_cpuinfo__add_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );

        // Process 2 releases CPUs 2 & 3, checks no victim and reclaims
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == 0  && victimlist[3] == 0 );
        assert( shmem_cpuinfo__recover_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
    }

    /*** Successful ping-pong, but using recover_all and return_all ***/
    {
        // Process 2 releases CPUs 2 & 3
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == 0 && victimlist[3] == 0 );

        // Process 1 wants CPUs 2 & 3
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 2 reclaims all
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__recover_all(p2_pid, victimlist) == DLB_NOTED );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 1 returns all
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__return_all(p1_pid, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p2_pid && victimlist[3] == p2_pid );

        // Process 2 releases CPUs 2 & 3 again
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 2 reclaims all again
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__recover_all(p2_pid, victimlist) == DLB_NOTED );
        assert( victimlist[2] == p1_pid && victimlist[3] == p1_pid );

        // Process 1 returns all and removes petition
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__return_all(p1_pid, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p2_pid && victimlist[3] == p2_pid );
        assert( shmem_cpuinfo__add_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );

        // Process 2 releases CPUs 2 & 3, checks no victim and reclaims
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == 0  && victimlist[3] == 0 );
        assert( shmem_cpuinfo__recover_all(p2_pid, victimlist) == DLB_SUCCESS );
    }

    /*** Late reply ***/
    {
        // Process 1 wants CPUs 2 & 3
        memset(victimlist, 0, sizeof(victimlist));
        assert( shmem_cpuinfo__acquire_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_NOTED );
        assert( victimlist[2] == 0  && victimlist[3] == 0 );

        // Process 1 no longer wants CPUs 2 & 3
        assert( shmem_cpuinfo__add_cpu_mask(p1_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == 0  && victimlist[3] == 0 );

        // Process 2 releases CPUs 2 & 3
        assert( shmem_cpuinfo__add_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == 0  && victimlist[3] == 0 );

        // Process 2 reclaims CPUs 2 & 3
        assert( shmem_cpuinfo__recover_cpu_mask(p2_pid, &p2_mask, victimlist) == DLB_SUCCESS );
        assert( victimlist[2] == p2_pid && victimlist[3] == p2_pid );
    }



    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid) == DLB_SUCCESS );

    return 0;
}
