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
#include "apis/dlb_types.h"

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

    // Init
    assert( shmem_procinfo__init(pid, &process_mask, NULL, NULL) == DLB_SUCCESS );
    //// A second init would silently return success
    ////    but now, it increments subprocesses_attached and causes undefined behaviour
    //assert( shmem_procinfo__init(pid, &process_mask, NULL, NULL) == DLB_SUCCESS );

    // Check registered mask is the same as our process mask
    assert( shmem_procinfo__getprocessmask(pid, &new_mask, DLB_SYNC_QUERY) == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &new_mask) );

    // process mask should not be dirty
    assert( shmem_procinfo__polldrom(pid, &new_threads, NULL) == DLB_NOUPDT );

    // Check unknown pid
    assert( shmem_procinfo__getprocessmask(pid+1, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOPROC );
    assert( shmem_procinfo__polldrom(pid+1, NULL, NULL) == DLB_ERR_NOPROC );

    // Finalize
    assert( shmem_procinfo__finalize(pid, false, NULL) == DLB_SUCCESS );
    // A second finalize should return error
    assert( shmem_procinfo__finalize(pid, false, NULL) == DLB_ERR_NOSHMEM );

    return 0;
}
