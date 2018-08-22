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

#include "unique_shmem.h"

#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <assert.h>

/* Thread binding tests involving CPU ownership changes */

int main(int argc, char *argv[]) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

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

    // Init P1
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == true );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == true );

    // P1 temporarily acquires CPUs 2 and 3
    CPU_SET(2, &p1_mask);
    CPU_SET(3, &p1_mask);
    shmem_cpuinfo__update_ownership(p1_pid, &p1_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 2) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 2) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 3) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 3) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 2) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 3) == false );

    // Reduce p1 mask to its original
    CPU_CLR(2, &p1_mask);
    CPU_CLR(3, &p1_mask);
    shmem_cpuinfo__update_ownership(p1_pid, &p1_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == false );

    // Init P2
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == true );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == true );

    // Get initial rebindings (disordered calls should not matter)
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 1) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == false );

    // Reduce p2 mask (thread 1)
    CPU_CLR(3, &p2_mask);
    shmem_cpuinfo__update_ownership(p2_pid, &p2_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );

    // Restore p2 mask
    CPU_SET(3, &p2_mask);
    shmem_cpuinfo__update_ownership(p2_pid, &p2_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == true );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 1) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == false );

    // Reduce p2 mask (thread 0, needs reassigning)
    CPU_CLR(2, &p2_mask);
    shmem_cpuinfo__update_ownership(p2_pid, &p2_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );

    // P1 acquires CPU 2
    CPU_SET(2, &p1_mask);
    shmem_cpuinfo__update_ownership(p1_pid, &p1_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 2) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 2) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 1) == false );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 2) == false );

    // Reduce p1 mask
    CPU_CLR(1, &p1_mask);
    CPU_CLR(2, &p1_mask);
    shmem_cpuinfo__update_ownership(p1_pid, &p1_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == true );
    assert( shmem_cpuinfo__get_thread_binding(    p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p1_pid, 0) == false );

    // P2 does not need rebinding, but can ask for it
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );

    // P2 acquires CPUs 1 and 2
    CPU_SET(1, &p2_mask);
    CPU_SET(2, &p2_mask);
    shmem_cpuinfo__update_ownership(p2_pid, &p2_mask);
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == true );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == true );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 2) == true );
    /* thread 0 is not reassigned */
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 0) == 3 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 0) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 1) == 1 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 1) == false );
    assert( shmem_cpuinfo__get_thread_binding(    p2_pid, 2) == 2 );
    assert( shmem_cpuinfo__thread_needs_rebinding(p2_pid, 2) == false );

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );

    return 0;
}
