/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 1 sub-process

int main( int argc, char **argv ) {

    int new_threads = 0;
    pid_t pid = getpid();
    cpu_set_t process_mask, new_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    // Check shmem not initialized
    assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    assert( shmem_procinfo__polldrom(pid, NULL, NULL) == DLB_ERR_NOSHMEM );

    // Init with too many CPUs
    CPU_SET(mu_get_system_size(), &process_mask);
    assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_ERR_PERM );
    // Init (good)
    CPU_CLR(mu_get_system_size(), &process_mask);
    assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    // A second init for the same pid should fail
    assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_ERR_INIT );

    // Check registered mask is the same as our process mask
    assert( shmem_procinfo__getprocessmask(pid, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &new_mask) );

    // process mask should not be dirty
    assert( shmem_procinfo__polldrom(pid, &new_threads, NULL) == DLB_NOUPDT );

    // Check unknown pid
    assert( shmem_procinfo__getprocessmask(pid+1, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOPROC );
    assert( shmem_procinfo__polldrom(pid+1, NULL, NULL) == DLB_ERR_NOPROC );

    // Check shmem cannot be inherited by another pid, since it's not flagged as preregistered
    pid_t new_pid = pid+1;
    assert( shmem_procinfo__init(new_pid, pid, &process_mask, NULL, SHMEM_KEY) == DLB_ERR_INIT );

    // Finalize
    assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_SUCCESS );
    // A second finalize should return error
    assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_ERR_NOSHMEM );

    // Test init with an empty mask
    CPU_ZERO(&process_mask);
    assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    shmem_procinfo__print_info(SHMEM_KEY);
    assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_SUCCESS );

    // Test init sharing CPUs
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    assert( shmem_procinfo__init_with_cpu_sharing(111, 0, &process_mask, SHMEM_KEY)
            == DLB_SUCCESS );
    assert( shmem_procinfo__init_with_cpu_sharing(222, 0, &process_mask, SHMEM_KEY)
            == DLB_SUCCESS );
    assert( shmem_procinfo__getprocessmask(111, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &new_mask) );
    assert( shmem_procinfo__getprocessmask(222, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &new_mask) );
    assert( shmem_procinfo__finalize(111, false, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(222, false, SHMEM_KEY) == DLB_SUCCESS );
    // Subsequent "regular inits" with shared masks should still fail
    assert( shmem_procinfo__init(111, 0, &process_mask, NULL, SHMEM_KEY)
            == DLB_SUCCESS );
    assert( shmem_procinfo__init(222, 0, &process_mask, NULL, SHMEM_KEY)
            == DLB_ERR_PERM );
    // Mixed "inits" will fail
    cpu_set_t empty_mask = {};
    assert( shmem_procinfo__init_with_cpu_sharing(333, 0, &empty_mask, SHMEM_KEY)
            == DLB_ERR_NOCOMP );
    assert( shmem_procinfo__finalize(111, false, SHMEM_KEY) == DLB_SUCCESS );

    return 0;
}
