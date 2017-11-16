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
    pid_t new_guests[ncpus];
    pid_t victims[ncpus];
    int cpus_priority_array[ncpus];
    int i;

    // Setup dummy priority CPUs
    for (i=0; i<ncpus; ++i) cpus_priority_array[i] = i;

    // Init
    assert( shmem_cpuinfo__init(pid, &process_mask, NULL) == DLB_SUCCESS );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Reclaim CPU
    assert( shmem_cpuinfo__reclaim_cpu(pid, 0, &new_guests[0], &victims[0]) == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    assert( victims[0] == -1 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Reclaim CPUs
    assert( shmem_cpuinfo__reclaim_cpus(pid, 1, new_guests, victims) == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    assert( victims[0] == -1 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Acquire CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, 0, &new_guests[0], &victims[0]) == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    assert( victims[0] == -1 );
    assert( shmem_cpuinfo__acquire_cpu(pid, 0, &new_guests[0], &victims[0]) == DLB_NOUPDT );
    assert( new_guests[0] == -1 );
    assert( victims[0] == -1 );

    // Lend CPU
    assert( shmem_cpuinfo__lend_cpu(pid, 0, &victims[0]) == DLB_SUCCESS );
    assert( victims[0] == 0 );

    // Borrow CPUs
    assert( shmem_cpuinfo__borrow_cpus(pid, PRIO_ANY, cpus_priority_array, 1, new_guests)
            == DLB_SUCCESS );
    assert( new_guests[0] == pid );
    for (i=1; i<ncpus; ++i) { assert( new_guests[i] == -1 ); }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == 0 ); }

    // Reclaim mask
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &process_mask, new_guests, victims) >= 0 );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == pid ); }
    for (i=0; i<ncpus; ++i) { assert( victims[i] == -1 ); }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == 0 ); }

    // Reclaim all
    assert( shmem_cpuinfo__reclaim_all(pid, new_guests, victims) >= 0 );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == pid ); }
    for (i=0; i<ncpus; ++i) { assert( victims[i] == -1 ); }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == 0 ); }

    // Acquire mask
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &process_mask, new_guests, victims) >= 0 );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == pid ); }
    for (i=0; i<ncpus; ++i) { assert( victims[i] == -1 ); }

    // Lend mask
    assert( shmem_cpuinfo__lend_cpu_mask(pid, &process_mask, new_guests) == DLB_SUCCESS );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == 0 ); }

    // Borrow all
    assert( shmem_cpuinfo__borrow_all(pid, PRIO_ANY, cpus_priority_array, new_guests) >= 0 );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == pid ); }

    // Return
    assert( shmem_cpuinfo__return_all(pid, new_guests) == DLB_NOUPDT );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == -1 ); }
    assert( shmem_cpuinfo__return_cpu(pid, 0, &new_guests[0]) == DLB_NOUPDT );
    assert( new_guests[0] == -1 );
    assert( shmem_cpuinfo__return_cpu_mask(pid, &process_mask, new_guests) == DLB_NOUPDT );
    for (i=0; i<ncpus; ++i) { assert( new_guests[i] == -1 ); }

    // Check errors with a nonexistent CPU
    assert( shmem_cpuinfo__acquire_cpu(pid, ncpus, &new_guests[0], &victims[0]) == DLB_ERR_PERM );
    assert( shmem_cpuinfo__reclaim_cpu(pid, ncpus, &new_guests[0], &victims[0]) == DLB_ERR_PERM );
    CPU_ZERO(&mask);
    CPU_SET(ncpus, &mask);
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__acquire_cpu_mask(pid, &mask, new_guests, victims) == DLB_SUCCESS );
    // mask is not checked beyond max_cpus
    assert( shmem_cpuinfo__reclaim_cpu_mask(pid, &mask, new_guests, victims) == DLB_NOUPDT );

    // Finalize
    assert( shmem_cpuinfo__finalize(pid) == DLB_SUCCESS );

    return 0;
}
