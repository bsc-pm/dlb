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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <sys/types.h>
/* #include <unistd.h> */
/* #include <stdio.h> */
/* #include <assert.h> */

int main(int argc, char *argv[]) {
    /* System size */
    enum { SYS_SIZE = 64 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);
    pid_t new_guests[SYS_SIZE];
    pid_t victims[SYS_SIZE];

    /* Initialize local masks */
    pid_t p1_pid = 11111;
    cpu_set_t p1_mask = { .__bits = {0x000300ff0000ffff} }; /* [ 0-15,32-39,48,49 ] */
    pid_t p2_pid = 22222;
    cpu_set_t p2_mask = { .__bits = {0x000cff00ffff0000} }; /* [ 16-31,40-47,50,51 ] */

    /* Initialize shared memories */
    assert( shmem_cpuinfo__init(p1_pid, &p1_mask, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, &p2_mask, NULL) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p1_pid, &p1_mask, NULL, NULL) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p2_pid, &p2_mask, NULL, NULL) == DLB_SUCCESS );

    /* CPUs 0-3 are lent */
    cpu_set_t cpus_to_lend = { .__bits = {0xf} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_lend, new_guests) == DLB_SUCCESS );

    /* CPUs 8-11 are lent and guested by P2 */
    cpu_set_t cpus_for_p2 = { .__bits = {0xf00} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_for_p2, new_guests) == DLB_SUCCESS );
    assert( shmem_cpuinfo__borrow_cpu_mask(p2_pid, &cpus_for_p2, new_guests) == DLB_SUCCESS );

    /* CPUs 12-15 are lent, guested by P2 and reclaimed */
    cpu_set_t cpus_to_reclaim = { .__bits = {0xf000} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_reclaim, new_guests) == DLB_SUCCESS );
    assert( shmem_cpuinfo__borrow_cpu_mask(p2_pid, &cpus_to_reclaim, new_guests) == DLB_SUCCESS );
    assert( shmem_cpuinfo__reclaim_cpu_mask(p1_pid, &cpus_to_reclaim, new_guests, victims)
            == DLB_NOTED );

    /* Print */
    shmem_cpuinfo__print_info(NULL, 4, true);
    shmem_procinfo__print_info(NULL);

    /* Finalize shared memories */
    assert( shmem_cpuinfo__finalize(p1_pid, NULL) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, NULL) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p1_pid, false, NULL) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, NULL) == DLB_SUCCESS );

    return 0;
}
