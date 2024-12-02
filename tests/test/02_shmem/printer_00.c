/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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
#include "support/types.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

/* array_cpuinfo_task_t */
#define ARRAY_T cpuinfo_task_t
#define ARRAY_KEY_T pid_t
#include "support/array_template.h"

int main(int argc, char *argv[]) {

    enum { SHMEM_SIZE_MULTIPLIER = 1 };
    enum { SHMEM_TALP_SIZE_MULTIPLIER = 10 };

    /* System size */
    enum { SYS_SIZE = 64 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);
    array_cpuinfo_task_t tasks;
    array_cpuinfo_task_t_init(&tasks, SYS_SIZE);

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
    array_cpuid_t cpus_priority_array;
    array_cpuid_t_init(&cpus_priority_array, SYS_SIZE);
    int64_t last_borrow_value = 0;
    int64_t *last_borrow = &last_borrow_value;
    int requested_ncpus;

    /* Initialize shared memories */
    assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(p2_pid, 0, &p2_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p1_pid, 0, &p1_mask, NULL, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
    shmem_barrier__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
    barrier_t *default_barrier = shmem_barrier__register("Barrier 0", 0);
    assert( default_barrier != NULL );
    barrier_t *barrier1 = shmem_barrier__register("Barrier 1", DLB_BARRIER_LEWI_OFF);
    assert( barrier1 != NULL );
    /* p1_pid and p2_pid */
    assert( shmem_talp__init(SHMEM_KEY, SHMEM_TALP_SIZE_MULTIPLIER) == DLB_SUCCESS );
    assert( shmem_talp__init(SHMEM_KEY, SHMEM_TALP_SIZE_MULTIPLIER) == DLB_SUCCESS );

    /* Enable request queues */
    shmem_cpuinfo__enable_request_queues();

    /* P2 requests 12 CPUs in the subset [4-15] */
    requested_ncpus = 12;
    array_cpuid_t_clear(&cpus_priority_array);
    for (int i=4; i<4+requested_ncpus; ++i) array_cpuid_t_push(&cpus_priority_array, i);
    assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p2_pid, &requested_ncpus,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                last_borrow, &tasks) == DLB_NOTED );
    assert( tasks.count == 0 );

    /* CPUs 0-3 are lent (they remain idle) */
    cpu_set_t cpus_to_lend = { .__bits = {0xf} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_lend, &tasks) == DLB_SUCCESS );

    /* CPUs 8-11 are lent and guested by P2 */
    cpu_set_t cpus_for_p2 = { .__bits = {0xf00} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_for_p2, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 4 );
    for (int i=0; i<4; ++i) {
        assert( tasks.items[i].pid == p2_pid && tasks.items[i].cpuid == i+8
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);

    /* CPUs 12-15 are lent, guested by P2 and reclaimed */
    cpu_set_t cpus_to_reclaim = { .__bits = {0xf000} };
    assert( shmem_cpuinfo__lend_cpu_mask(p1_pid, &cpus_to_reclaim, &tasks) == DLB_SUCCESS );
    assert( tasks.count == 4 );
    for (int i=0; i<4; ++i) {
        assert( tasks.items[i].pid == p2_pid && tasks.items[i].cpuid == i+12
                && tasks.items[i].action == ENABLE_CPU );
    }
    array_cpuinfo_task_t_clear(&tasks);
    assert( shmem_cpuinfo__reclaim_cpu_mask(p1_pid, &cpus_to_reclaim, &tasks) == DLB_NOTED );
    assert( tasks.count == 8 ); /* 4 enable for p1_pid, 4 disable for p2_pid */
    array_cpuinfo_task_t_clear(&tasks);

    /* P1 requests 4 CPUs in the subset [16-19] */
    requested_ncpus = 4;
    array_cpuid_t_clear(&cpus_priority_array);
    for (int i=16; i<16+requested_ncpus; ++i) array_cpuid_t_push(&cpus_priority_array, i);
    assert( shmem_cpuinfo__acquire_ncpus_from_cpu_subset(p1_pid, &requested_ncpus,
                &cpus_priority_array, LEWI_AFFINITY_AUTO, 0 /* max_parallelism */,
                last_borrow, &tasks) == DLB_NOTED );
    assert( tasks.count == 0 );

    /* P3 registers one CPU and requests CPU 19 also */
    pid_t p3_pid = 33333;
    cpu_set_t p3_mask;
    mu_parse_mask("63", &p3_mask);
    assert( shmem_cpuinfo__init(p3_pid, 0, &p3_mask, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_procinfo__init(p3_pid, 0, &p3_mask, NULL, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
    assert( shmem_cpuinfo__acquire_cpu(p3_pid, 19, &tasks) == DLB_NOTED );
    assert( tasks.count == 0 );

    /* Register some TALP regions */
    int region_id1 = -1;
    assert( shmem_talp__register(p1_pid, 1, "Custom region 1", &region_id1) == DLB_SUCCESS );
    assert( region_id1 == 0 );
    assert( shmem_talp__set_times(region_id1, 111111, 111111) == DLB_SUCCESS );
    int region_id2 = -1 ;
    assert( shmem_talp__register(p1_pid, 1, "Region 2", &region_id2) == DLB_SUCCESS );
    assert( region_id2 == 1 );
    assert( shmem_talp__set_times(region_id2, INT64_MAX, 42) == DLB_SUCCESS );
    int region_id3 = -1;
    assert( shmem_talp__register(p2_pid, 1, "Custom region 1", &region_id3) == DLB_SUCCESS );
    assert( region_id3 == 2 );
    assert( shmem_talp__set_times(region_id3, 0, 4242) == DLB_SUCCESS );

    /* Print */
    shmem_cpuinfo__print_info(NULL, 0, 0, true);
    shmem_procinfo__print_info(NULL, SHMEM_SIZE_MULTIPLIER);
    shmem_barrier__print_info(NULL, SHMEM_SIZE_MULTIPLIER);
    shmem_talp__print_info(NULL, SHMEM_TALP_SIZE_MULTIPLIER);

    /* Finalize shared memories */
    assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(p3_pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
            == DLB_SUCCESS );
    assert( shmem_barrier__detach(default_barrier) == 0 );
    assert( shmem_barrier__detach(barrier1) == 0 );
    shmem_barrier__finalize(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
    assert( shmem_talp__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_talp__finalize(p2_pid) == DLB_SUCCESS );

    return 0;
}
