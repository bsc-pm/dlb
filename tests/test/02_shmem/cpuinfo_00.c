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
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"

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
    int ncpus = CPU_COUNT(&process_mask);
    pid_t victimlist[ncpus];
    int i;

    // Init
    assert( shmem_cpuinfo__init(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Add CPU
    victimlist[0] = -1;
    assert( shmem_cpuinfo__add_cpu(pid, 0, &victimlist[0]) == DLB_SUCCESS );
    assert( victimlist[0] == -1 );

    // Recover CPU
    assert( shmem_cpuinfo__recover_cpu(pid, 0, &victimlist[0]) == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Add CPU
    assert( shmem_cpuinfo__add_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Recover CPUs
    assert( shmem_cpuinfo__recover_cpus(pid, 1, victimlist) == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Add CPU
    assert( shmem_cpuinfo__add_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Collect CPU
    assert( shmem_cpuinfo__collect_cpu(pid, 0, victimlist) == DLB_SUCCESS );
    assert( victimlist[0] == pid );
    assert( shmem_cpuinfo__collect_cpu(pid, 0, victimlist) == DLB_NOUPDT );

    // Add CPU
    assert( shmem_cpuinfo__add_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Collect CPUs
    assert( shmem_cpuinfo__collect_cpus(pid, 1, victimlist) == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Add mask
    assert( shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Recover mask
    assert( shmem_cpuinfo__recover_cpu_mask(pid, &process_mask, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Add mask
    assert( shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Recover all
    assert( shmem_cpuinfo__recover_all(pid, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Add mask
    assert( shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Collect mask
    assert( shmem_cpuinfo__collect_cpu_mask(pid, &process_mask, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Add mask
    assert( shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Collect all
    assert( shmem_cpuinfo__collect_all(pid, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Return
    assert( shmem_cpuinfo__return_all(pid, NULL) == DLB_NOUPDT );
    assert( shmem_cpuinfo__return_cpu(pid, 0, NULL) == DLB_NOUPDT );
    assert( shmem_cpuinfo__return_cpu_mask(pid, &process_mask, NULL) == DLB_NOUPDT );

    // Check errors with a nonexistent CPU
    assert( shmem_cpuinfo__collect_cpu(pid, ncpus, NULL) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__recover_cpu(pid, ncpus, NULL) == DLB_ERR_PERM );
    CPU_ZERO(&mask);
    CPU_SET(ncpus, &mask);
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__collect_cpu_mask(pid, &mask, NULL) == DLB_SUCCESS );
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__recover_cpu_mask(pid, &mask, NULL) == DLB_NOUPDT );

    // Finalize
    assert( shmem_cpuinfo__finalize(pid) == DLB_SUCCESS );

    return 0;
}
