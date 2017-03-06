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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 2 sub-processes

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
    pid_t victim;

    // Init
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p1_mask, NULL) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );


    /*** Successful ping-pong ***/
    {
        // Process 1 wants CPU 3
        victim = 0;
        assert( shmem_cpuinfo__collect_cpu(p1_pid, 3, &victim) == DLB_NOTED );
        assert( victim == 0 );

        // Process 2 releases CPU 3
        victim = 0;
        assert( shmem_cpuinfo__add_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == p1_pid );

        // Process 2 reclaims CPU 3
        assert( shmem_cpuinfo__recover_cpu(p1_pid, 3, &victim) == DLB_ERR_PERM );
        victim = 0;
        assert( shmem_cpuinfo__recover_cpu(p2_pid, 3, &victim) == DLB_NOTED );
        assert( victim == p1_pid );

        // Process 1 returns CPU 3
        victim = 0;
        assert( shmem_cpuinfo__return_cpu(p1_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == p2_pid );

        // Process 2 releases CPU 3 again
        victim = 0;
        assert( shmem_cpuinfo__add_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == p1_pid );

        // Process 2 reclaims CPU 3 again
        victim = 0;
        assert( shmem_cpuinfo__recover_cpu(p2_pid, 3, &victim) == DLB_NOTED );
        assert( victim == p1_pid );

        // Process 1 returns CPU 3 and removes petition
        victim = 0;
        assert( shmem_cpuinfo__return_cpu(p1_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == p2_pid );
        assert( shmem_cpuinfo__add_cpu(p1_pid, 3, &victim) == DLB_SUCCESS );

        // Process 2 releases CPU 3, checks no victim and reclaims
        victim = 0;
        assert( shmem_cpuinfo__add_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == 0 );
        assert( shmem_cpuinfo__recover_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
    }


    /*** Late reply ***/
    {
        // Process 1 wants CPU 3
        victim = 0;
        assert( shmem_cpuinfo__collect_cpu(p1_pid, 3, &victim) == DLB_NOTED );
        assert( victim == 0 );

        // Process 1 no longer wants CPU 3
        assert( shmem_cpuinfo__add_cpu(p1_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == 0 );

        // Process 2 releases CPU 3
        assert( shmem_cpuinfo__add_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == 0 );

        // Process 2 reclaims CPU 3
        assert( shmem_cpuinfo__recover_cpu(p2_pid, 3, &victim) == DLB_SUCCESS );
        assert( victim == p2_pid );
    }

    /*** Errors ***/
    assert( shmem_cpuinfo__return_cpu(p1_pid, 3, NULL) == DLB_ERR_PERM );

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid) == DLB_SUCCESS );

    // The shared memory file should not exist at this point
    char shm_filename[SHM_NAME_LENGTH+8];
    snprintf(shm_filename, SHM_NAME_LENGTH+8, "/dev/shm/DLB_cpuinfo_%d", getuid());
    assert( access(shm_filename, F_OK) == -1 );

    return 0;
}
