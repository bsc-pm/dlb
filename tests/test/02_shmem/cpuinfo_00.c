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

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 1 sub-process

int main( int argc, char **argv ) {
    int error;
    pid_t pid = getpid();
    cpu_set_t process_mask, mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    int ncpus = CPU_COUNT(&process_mask);
    pid_t victimlist[ncpus];
    int i;

    // Init
    error = shmem_cpuinfo__init(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Add CPU
    pid_t new_owner = -1;
    error = shmem_cpuinfo__add_cpu(pid, 0, &new_owner);
    assert(error == DLB_SUCCESS);
    assert(new_owner == -1);

    // Recover CPU
    error = shmem_cpuinfo__recover_cpu(pid, 0, &new_owner);
    assert(error == DLB_SUCCESS);
    assert(new_owner == pid);

    // Add CPU
    error = shmem_cpuinfo__add_cpu(pid, 0, NULL);
    assert(error == DLB_SUCCESS);

    // Recover CPUs
    error = shmem_cpuinfo__recover_cpus(pid, 1, victimlist);
    assert(error == DLB_SUCCESS);
    assert(victimlist[0] == pid);

    // Add CPU
    error = shmem_cpuinfo__add_cpu(pid, 0, NULL);
    assert(error == DLB_SUCCESS);

    // Collect CPU
    error = shmem_cpuinfo__collect_cpu(pid, 0, victimlist);
    assert(error == DLB_SUCCESS);
    assert(victimlist[0] == pid);

    // Add CPU
    error = shmem_cpuinfo__add_cpu(pid, 0, NULL);
    assert(error == DLB_SUCCESS);

    // Collect CPUs
    error = shmem_cpuinfo__collect_cpus(pid, 1, victimlist);
    assert(error == DLB_SUCCESS);
    assert(victimlist[0] == pid);

    // Add mask
    error = shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Recover mask
    error = shmem_cpuinfo__recover_cpu_mask(pid, &process_mask, victimlist);
    for (i=0; i<ncpus; ++i) {
        assert(victimlist[i] == pid);
    }

    // Add mask
    error = shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Recover all
    error = shmem_cpuinfo__recover_all(pid, victimlist);
    for (i=0; i<ncpus; ++i) {
        assert(victimlist[i] == pid);
    }

    // Add mask
    error = shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Collect mask
    error = shmem_cpuinfo__collect_cpu_mask(pid, &process_mask, victimlist);
    for (i=0; i<ncpus; ++i) {
        assert(victimlist[i] == pid);
    }

    // Add mask
    error = shmem_cpuinfo__add_cpu_mask(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Collect all
    error = shmem_cpuinfo__collect_all(pid, victimlist);
    for (i=0; i<ncpus; ++i) {
        assert(victimlist[i] == pid);
    }

    // Check errors with nonexistent CPU
    error = shmem_cpuinfo__collect_cpu(pid, ncpus, NULL);
    assert(error == DLB_ERR_PERM);
    error = shmem_cpuinfo__recover_cpu(pid, ncpus, NULL);
    assert(error == DLB_ERR_PERM);
    CPU_ZERO(&mask);
    CPU_SET(ncpus, &mask);
    error = shmem_cpuinfo__collect_cpu_mask(pid, &mask, NULL);
    assert(error == DLB_SUCCESS); // mask is not checked beyond max_cpus
    error = shmem_cpuinfo__recover_cpu_mask(pid, &mask, NULL);
    assert(error == DLB_SUCCESS); // mask is not checked beyond max_cpus

    // Finalize
    error = shmem_cpuinfo__finalize(pid);
    assert(error == DLB_SUCCESS);

    // The shared memory file should not exist at this point
    char shm_filename[SHM_NAME_LENGTH+8];
    snprintf(shm_filename, SHM_NAME_LENGTH+8, "/dev/shm/DLB_cpuinfo_%d", getuid());
    assert(access(shm_filename, F_OK) == -1);

    return 0;
}
