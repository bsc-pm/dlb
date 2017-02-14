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

// Basic checks with 1 sub-process

int main( int argc, char **argv ) {
    int error;
    int new_threads = 0;
    pid_t pid = getpid();
    cpu_set_t process_mask, new_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    // Check shmem not initialized
    error = shmem_procinfo__getprocessmask(pid, NULL);
    assert(error == DLB_ERR_NOSHMEM);
    error = shmem_procinfo__polldrom(pid, NULL, NULL);
    assert(error == DLB_ERR_NOSHMEM);

    // Init
    error = shmem_procinfo__init(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);
    // A second init would silently return success
    error = shmem_procinfo__init(pid, &process_mask, NULL);
    assert(error == DLB_SUCCESS);

    // Check registered mask is the same as our process mask
    error = shmem_procinfo__getprocessmask(pid, &new_mask);
    assert(error == DLB_SUCCESS);
    assert(CPU_EQUAL(&process_mask, &new_mask));

    // process mask should not be dirty
    error = shmem_procinfo__polldrom(pid, &new_threads, NULL);
    assert(error == DLB_NOUPDT);

    // Check unknown pid
    error = shmem_procinfo__getprocessmask(pid+1, NULL);
    assert(error == DLB_ERR_NOPROC);
    error = shmem_procinfo__polldrom(pid+1, NULL, NULL);
    assert(error == DLB_ERR_NOPROC);

    // Finalize
    error = shmem_procinfo__finalize(pid);
    assert(error == DLB_SUCCESS);
    // A second finalize should return error
    error = shmem_procinfo__finalize(pid);
    assert(error == DLB_ERR_NOPROC);

    // The shared memory file should not exist at this point
    char shm_filename[SHM_NAME_LENGTH+8];
    snprintf(shm_filename, SHM_NAME_LENGTH+8, "/dev/shm/DLB_procinfo_%d", getuid());
    assert(access(shm_filename, F_OK) == -1);

    return 0;
}
