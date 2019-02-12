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
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 2+ sub-processes

int main( int argc, char **argv ) {
    pid_t pid = getpid();
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    // Check permission error with two subprocesses sharing mask
    {
        assert( shmem_procinfo__init(pid, &process_mask, NULL, NULL) == DLB_SUCCESS );
        if (num_cpus > 1) {
            assert( shmem_procinfo__init(pid+1, &process_mask, NULL, NULL) == DLB_ERR_PERM );
        }
        assert( shmem_procinfo__finalize(pid, false, NULL) == DLB_SUCCESS );

        // Check that the shared memory has been finalized
        assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    }

    // Check shared memory capacity
    {
        // This loop should completely fill the shared memory
        int cpuid;
        for (cpuid=0; cpuid<num_cpus; ++cpuid) {
            CPU_ZERO(&process_mask);
            CPU_SET(cpuid, &process_mask);
            assert( shmem_procinfo__init(pid+cpuid, &process_mask, NULL, NULL) == DLB_SUCCESS );
        }

        // Another initialization should return error
        CPU_ZERO(&process_mask);
        assert( shmem_procinfo__init(pid+cpuid, &process_mask, NULL, NULL) == DLB_ERR_NOMEM );

        // Finalize all
        for (cpuid=0; cpuid<num_cpus; ++cpuid) {
            assert( shmem_procinfo__finalize(pid+cpuid, false, NULL) == DLB_SUCCESS );
        }

        // Check that the shared memory has been finalized
        assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    }

    // Check that each subprocess finalizes its own part of the shared memory
    {
        // Initialize as many subprocess as number of CPUs
        int i;
        for (i=0; i<num_cpus; ++i) {
            CPU_ZERO(&process_mask);
            CPU_SET(i, &process_mask);
            assert( shmem_procinfo__init(i+1, &process_mask, NULL, NULL) == DLB_SUCCESS );
        }

        // subprocess 1 tries to finalize as many times as number of CPUs
        assert( shmem_procinfo__finalize(1, false, NULL) == DLB_SUCCESS );
        for (i=1; i<num_cpus; ++i) {
            assert( shmem_procinfo__finalize(1, false, NULL) == DLB_ERR_NOPROC );
        }

        // rest of subprocesses should finalize correctly
        for (i=1; i<num_cpus; ++i) {
            assert( shmem_procinfo__finalize(i+1, false, NULL) == DLB_SUCCESS );
        }

        // Check that the shared memory has been finalized
        assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    }


    return 0;
}
