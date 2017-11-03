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
    int cpus_priority_array[ncpus];
    int i;

    // Setup dummy priority CPUs
    for (i=0; i<ncpus; ++i) cpus_priority_array[i] = i;

    // Init
    assert( shmem_cpuinfo__init(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Lend CPU
    victimlist[0] = -1;
    assert( shmem_cpuinfo__lend_cpu(pid, 0, &victimlist[0]) == DLB_SUCCESS );
    assert( victimlist[0] == -1 );

    // Reclaim CPU
    assert( shmem_cpuinfo__reclaim_cpu(pid, 0, &victimlist[0]) == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Reclaim CPUs
    assert( shmem_cpuinfo__reclaim_cpus(pid, 1, victimlist) == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Acquire CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, 0, victimlist) == DLB_SUCCESS );
    assert( victimlist[0] == pid );
    assert( shmem_cpuinfo__acquire_cpu(pid, 0, victimlist) == DLB_NOUPDT );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, NULL) == DLB_SUCCESS );

    // Borrow CPUs
    assert( shmem_cpuinfo__borrow_cpus(pid, PRIO_NONE, cpus_priority_array, 1, victimlist)
            == DLB_SUCCESS );
    assert( victimlist[0] == pid );

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Reclaim mask
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &process_mask, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Reclaim all
    assert( shmem_cpuinfo__reclaim_all(pid, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Acquire mask
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &process_mask, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Borrow all
    assert( shmem_cpuinfo__borrow_all(pid, PRIO_NONE, cpus_priority_array, victimlist) >= 0 );
    for (i=0; i<ncpus; ++i) {
        assert( victimlist[i] == pid );
    }

    // Return
    assert( shmem_cpuinfo__return_all(pid, NULL) == DLB_NOUPDT );
    assert( shmem_cpuinfo__return_cpu(pid, 0, NULL) == DLB_NOUPDT );
    assert( shmem_cpuinfo__return_cpu_mask(pid, &process_mask, NULL) == DLB_NOUPDT );

    // Check errors with a nonexistent CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, ncpus, NULL) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__reclaim_cpu(pid, ncpus, NULL) == DLB_ERR_PERM );
    CPU_ZERO(&mask);
    CPU_SET(ncpus, &mask);
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &mask, NULL) == DLB_SUCCESS );
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &mask, NULL) == DLB_NOUPDT );

    // Finalize
    assert( shmem_cpuinfo__finalize(pid) == DLB_SUCCESS );

    return 0;
}
