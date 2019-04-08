/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

/* Thread binding tests involving LeWI */

int main(int argc, char *argv[]) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    pid_t new_guest, victim;

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

    // Init
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );

    // Get initial rebindings (disordered calls should not matter)
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == 3 );

    // P2 lends CPU 3 (thread 1), CPU 2 (thread 0) does not need to rebind
    shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guest);
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == -1 );

    // P2 recovers CPU 3
    shmem_cpuinfo__acquire_cpu(p2_pid, 3, &new_guest, &victim);
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == 3 );

    // P2 lends CPU 2 (thread 0 needs reassigning)
    shmem_cpuinfo__lend_cpu(p2_pid, 2, &new_guest);
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 3 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == -1 );

    // P1 acquires CPU 2
    shmem_cpuinfo__acquire_cpu(p1_pid, 2, &new_guest, &victim);
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 2) == 2 );

    // P1 lends CPUs 1 and 2
    shmem_cpuinfo__lend_cpu(p1_pid, 1, &new_guest);
    shmem_cpuinfo__lend_cpu(p1_pid, 2, &new_guest);
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 0) == 0 );
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == -1 );
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 2) == -1 );

    // P2 bind check
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 3 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == -1 );

    // P2 acquires CPUs 1 and 2 (owned CPUs first)
    shmem_cpuinfo__acquire_cpu(p2_pid, 1, &new_guest, &victim);
    shmem_cpuinfo__acquire_cpu(p2_pid, 2, &new_guest, &victim);
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == 3 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 2) == 1 );

    // P1 acquires CPU 1 forcing oversubscription
    shmem_cpuinfo__acquire_cpu(p1_pid, 1, &new_guest, &victim);
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 2) == 1 );
    shmem_cpuinfo__lend_cpu(p2_pid, 1, &new_guest);
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == 1 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 2) == -1 );

    // P2 acquires CPUs 1 (lent by P1), 2 (reclaimed) and 3 (borrowed)
    shmem_cpuinfo__lend_cpu(p1_pid, 1, &new_guest);
    shmem_cpuinfo__acquire_cpu(p2_pid, 1, &new_guest, &victim); /* lent */
    shmem_cpuinfo__lend_cpu(p2_pid, 2, &new_guest);
    shmem_cpuinfo__acquire_cpu(p1_pid, 2, &new_guest, &victim);
    shmem_cpuinfo__acquire_cpu(p2_pid, 2, &new_guest, &victim); /* force OS */
    shmem_cpuinfo__lend_cpu(p2_pid, 3, &new_guest);
    shmem_cpuinfo__acquire_cpu(p2_pid, 3, &new_guest, &victim); /* borrow */
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 1) == 3 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 2) == 1 );
    /* P1 leaves and CPU 2 is no longer oversubscribed */
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == 2 );
    shmem_cpuinfo__lend_cpu(p1_pid, 2, &new_guest);
    assert( shmem_cpuinfo__get_thread_binding(p1_pid, 1) == -1 );
    assert( shmem_cpuinfo__get_thread_binding(p2_pid, 0) == 2 );

    // Finalize
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );

    return 0;
}
