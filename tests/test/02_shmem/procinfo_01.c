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
    char shm_filename[SHM_NAME_LENGTH+8];
    snprintf(shm_filename, SHM_NAME_LENGTH+8, "/dev/shm/DLB_procinfo_%d", getuid());

    // Register two processes with the same mask, and check the error codes are ok
    assert( shmem_procinfo__init(pid, &process_mask, NULL, NULL) == DLB_SUCCESS );
    if (num_cpus == 1) {
        assert( shmem_procinfo__init(pid+1, &process_mask, NULL, NULL) == DLB_ERR_NOMEM );
    } else {
        assert( shmem_procinfo__init(pid+1, &process_mask, NULL, NULL) == DLB_ERR_PERM );
    }
    assert( shmem_procinfo__finalize(pid) == DLB_SUCCESS );

    // The shared memory file should not exist at this point
    assert( access(shm_filename, F_OK) == -1 );

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
        assert( shmem_procinfo__finalize(pid+cpuid) == DLB_SUCCESS );
    }

    // The shared memory file should not exist at this point
    assert( access(shm_filename, F_OK) == -1 );

    return 0;
}
