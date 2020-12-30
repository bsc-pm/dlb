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

#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <assert.h>

// PreInit tests of two applications overlapping resources
// APP1:  ============X
//        ============X
//        ====|
//        ====|
//  APP2:   ================X
//          ================X
//                      ====X
//                      ====X

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    const cpu_set_t original_p1_mask = { .__bits = {0xf} }; /* [1111] */
    const cpu_set_t original_p2_mask = { .__bits = {0xc} }; /* [1100] */

    // Initialize external
    assert( shmem_procinfo_ext__init(SHMEM_KEY) == DLB_SUCCESS );

    // Pre-Initialize sub-process 1
    pid_t p1_pid = 111;
    cpu_set_t p1_mask;
    memcpy(&p1_mask, &original_p1_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, 0) == DLB_SUCCESS );

    // Pre-Initialize sub-process 2
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    memcpy(&p2_mask, &original_p2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo_ext__preinit(p2_pid, &p2_mask, DLB_STEAL_CPUS) == DLB_SUCCESS );

    // Initialize sub-process 2
    CPU_ZERO(&p2_mask);
    assert( shmem_procinfo__init(p2_pid, NULL, &p2_mask, SHMEM_KEY) == DLB_NOTED );
    assert( CPU_EQUAL(&original_p2_mask, &p2_mask) );

    // Initialize sub-process 1
    CPU_ZERO(&p1_mask);
    assert( shmem_procinfo__init(p1_pid, NULL, &p1_mask, SHMEM_KEY) == DLB_NOTED );
    cpu_set_t p1_mask_after;
    /* P1 needs to readjust its mask: [1111] - [1100] = [0011] */
    mu_substract(&p1_mask_after, &original_p1_mask, &original_p2_mask);
    assert( CPU_EQUAL(&p1_mask_after, &p1_mask) );

    // Finalize sub-process 1
    assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY) == DLB_SUCCESS );

    // Assign all CPUs to sub-process 2
    cpu_set_t new_p2_mask;
    CPU_OR(&new_p2_mask, &original_p1_mask, &original_p2_mask);
    assert( shmem_procinfo__setprocessmask(p2_pid, &new_p2_mask, 0) == DLB_SUCCESS );

    // Sub-process 2 polls
    int ncpus = 0;
    assert( shmem_procinfo__polldrom(p2_pid, &ncpus, &p2_mask) == DLB_SUCCESS );
    assert( ncpus == CPU_COUNT(&new_p2_mask) );
    assert( CPU_EQUAL(&new_p2_mask, &p2_mask) );

    // Finalize sub-process 2
    assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY) == DLB_SUCCESS );

    // Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    return 0;
}
