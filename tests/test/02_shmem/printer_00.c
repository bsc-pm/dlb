/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_talp.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    /* System size */
    enum { SYS_SIZE = 64 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);
    pid_t new_guests[SYS_SIZE];
    pid_t victims[SYS_SIZE];
    int i, j;

    /* Initialize local masks */
    pid_t p1_pid = 11111;
    cpu_set_t p1_mask;
    mu_parse_mask("0-15,32-39,48,49", &p1_mask);
    pid_t p2_pid = 22222;
    cpu_set_t p2_mask;
    mu_parse_mask("16-31,40-47,50,51", &p2_mask);

    /* check masks correctness */
    assert( CPU_COUNT(&p1_mask) == 26 );
    assert( CPU_COUNT(&p2_mask) == 26 );
    cpu_set_t p1_p2_mask;
    CPU_OR(&p1_p2_mask, &p1_mask, &p2_mask);
    assert( CPU_COUNT(&p1_p2_mask) == 52 );
    CPU_AND(&p1_p2_mask, &p1_mask, &p2_mask);
    assert( CPU_COUNT(&p1_p2_mask) == 0 );

    /* Other shmem parameters */
    int cpus_priority_array[SYS_SIZE];
    int64_t last_borrow_value = 0;
    int64_t *last_borrow = &last_borrow_value;
    int requested_ncpus;
    int barrier_id = 0;

    /* Initialize shared memories */
    assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p1_pid, 0, &p1_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    shmem_barrier__init(SHMEM_KEY);
    shmem_barrier__attach(barrier_id, true);
    assert( shmem_talp__init(SHMEM_KEY, 0) == DLB_SUCCESS );    /* p1_pid and p2_pid */
    assert( shmem_talp__init(SHMEM_KEY, 0) == DLB_SUCCESS );

    /* Enable request queues */
    shmem_cpuinfo__enable_request_queues();

    /* P2 requests 12 CPUs in the subset [4-15] */
    requested_ncpus = 12;
    for (i=0, j=4; i<requested_ncpus; ++i) cpus_priority_array[i] = j++;
    cpus_priority_array[i] = -1;
    assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus, cpus_priority_array,
            PRIO_ANY, 0 /* max_parallelism */, last_borrow, new_guests, victims) == DLB_NOTED );

    /* CPUs 0-3 are lent (they remain idle) */
    cpu_set_t cpus_to_lend = { .__bits = {0xf} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_lend, new_guests) == DLB_SUCCESS );

    /* CPUs 8-11 are lent and guested by P2 */
    cpu_set_t cpus_for_p2 = { .__bits = {0xf00} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_for_p2, new_guests) == DLB_SUCCESS );
    for (i=8; i<12; ++i) assert( new_guests[i] == p2_pid );

    /* CPUs 12-15 are lent, guested by P2 and reclaimed */
    cpu_set_t cpus_to_reclaim = { .__bits = {0xf000} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_reclaim, new_guests) == DLB_SUCCESS );
    for (i=12; i<16; ++i) assert( new_guests[i] == p2_pid );
    assert( shmem_cpuinfo__reclaim_cpu_mask(p1_pid, &cpus_to_reclaim, new_guests, victims)
            == DLB_NOTED );

    /* P1 requests 4 CPUs in the subset [16-19] */
    requested_ncpus = 4;
    for (i=0, j=16; i<requested_ncpus; ++i) cpus_priority_array[i] = j++;
    cpus_priority_array[i] = -1;
    assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus, cpus_priority_array,
            PRIO_ANY, 0 /* max_parallelism */, last_borrow, new_guests, victims) == DLB_NOTED );

    /* P3 registers one CPU and requests CPU 19 also */
    pid_t p3_pid = 33333;
    cpu_set_t p3_mask;
    mu_parse_mask("63", &p3_mask);
    assert( shmem_cpuinfo__init(p3_pid, 0, &p3_mask, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p3_pid, 0, &p3_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__acquire_cpu(p3_pid, 19, new_guests, victims) == DLB_NOTED );

    /* Register some TALP regions */
    int region_id1 = -1;
    assert( shmem_talp__register(p1_pid, "Custom region 1", &region_id1) == DLB_SUCCESS );
    assert( region_id1 == 0 );
    assert( shmem_talp__set_times(region_id1, 111111, 111111) == DLB_SUCCESS );
    int region_id2 = -1 ;
    assert( shmem_talp__register(p1_pid, "Region 2", &region_id2) == DLB_SUCCESS );
    assert( region_id2 == 1 );
    assert( shmem_talp__set_times(region_id2, INT64_MAX, 42) == DLB_SUCCESS );
    int region_id3 = -1;
    assert( shmem_talp__register(p2_pid, "Custom region 1", &region_id3) == DLB_SUCCESS );
    assert( region_id3 == 2 );
    assert( shmem_talp__set_times(region_id3, 0, 4242) == DLB_SUCCESS );

    /* Print */
    shmem_cpuinfo__print_info(NULL, 0, true);
    shmem_procinfo__print_info(NULL);
    shmem_barrier__print_info(NULL);
    shmem_talp__print_info(NULL, 0);

    /* Finalize shared memories */
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p3_pid, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY) == DLB_SUCCESS );
    shmem_barrier__detach(barrier_id);
    shmem_barrier__finalize(SHMEM_KEY);
    assert( shmem_talp__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_talp__finalize(p2_pid) == DLB_SUCCESS );

    return 0;
}
