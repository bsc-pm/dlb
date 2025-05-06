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

#include "LB_numThreads/omptm_omp5.h"
#include "LB_numThreads/numThreads.h"
#include "LB_numThreads/omptool.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_sp.h"
#include "support/mask_utils.h"

#include <assert.h>
#include <sched.h>
#include <string.h>

/* array_cpuid_t */
#define ARRAY_T cpuid_t
#include "support/array_template.h"

// This test needs at least room for 8 CPUs
enum { SYS_SIZE = 8 };

static int num_omp_threads = 0;

void omp_set_num_threads(int num_threads) {
    num_omp_threads = num_threads;
}

int main (int argc, char *argv[]) {

    /* Simulate a 4 core system with 2 CPUs per core with its bindings in a
     * round-robin fashion */
    enum { NUM_CORES = 4 };
    enum { NUM_NODES = 1 };
    cpu_set_t system_mask;
    cpu_set_t node_masks[NUM_NODES];
    cpu_set_t core_masks[NUM_CORES];
    mu_parse_mask("0-7", &system_mask);
    mu_parse_mask("0-7", &node_masks[0]);
    mu_parse_mask("0,4", &core_masks[0]);
    mu_parse_mask("1,5", &core_masks[1]);
    mu_parse_mask("2,6", &core_masks[2]);
    mu_parse_mask("3,7", &core_masks[3]);
    mu_testing_set_sys_masks(&system_mask, core_masks, NUM_CORES, node_masks, NUM_NODES);

    char options[64] = "--lewi --shm-key=";
    strcat(options, SHMEM_KEY);

    /* P1 init */
    cpu_set_t p1_mask;
    mu_parse_mask("0,1,4,5", &p1_mask);
    subprocess_descriptor_t *spd1 = DLB_Init_sp(0, &p1_mask, options);
    assert( spd1 != NULL );
    pid_t p1_pid = spd1->id;

    /* P2 init */
    cpu_set_t p2_mask;
    mu_parse_mask("2,3,6,7", &p2_mask);
    subprocess_descriptor_t *spd2 = DLB_Init_sp(0, &p2_mask, options);
    assert( spd2 != NULL );

    /* P1 is the active subprocess */
    spd_enter_dlb(spd1);

    /* P1 inits omp5 thread manager with OMP_PLACES="threads" */
    {
        setenv("OMP_PLACES", "threads", 1);
        omptm_omp5__init(p1_pid, &spd1->options);
        omptm_omp5_testing__set_num_threads_fn(omp_set_num_threads);

        const array_cpuid_t *cpu_bindings = omptm_omp5_testing__compute_and_get_cpu_bindings();
        assert( cpu_bindings->count == 4 );
        assert( cpu_bindings->items[0] == 0);
        assert( cpu_bindings->items[1] == 4);
        assert( cpu_bindings->items[2] == 1);
        assert( cpu_bindings->items[3] == 5);

        /* P1 gets P2's CPUs */
        enable_cpu_set(&spd1->pm, &p2_mask);
        assert( num_omp_threads == 8 );

        cpu_bindings = omptm_omp5_testing__compute_and_get_cpu_bindings();
        assert( cpu_bindings->count == 8 );
        assert( cpu_bindings->items[0] == 0);
        assert( cpu_bindings->items[1] == 4);
        assert( cpu_bindings->items[2] == 1);
        assert( cpu_bindings->items[3] == 5);
        assert( cpu_bindings->items[4] == 2);
        assert( cpu_bindings->items[5] == 6);
        assert( cpu_bindings->items[6] == 3);
        assert( cpu_bindings->items[7] == 7);

        /* P1 reset */
        disable_cpu_set(&spd1->pm, &p2_mask);
        assert( num_omp_threads == 4 );

        omptm_omp5__finalize();
    }

    /* P1 inits omp5 thread manager with OMP_PLACES="cores" */
    {
        setenv("OMP_PLACES", "cores", 1);
        omptm_omp5__init(p1_pid, &spd1->options);
        omptm_omp5_testing__set_num_threads_fn(omp_set_num_threads);

        const array_cpuid_t *cpu_bindings = omptm_omp5_testing__compute_and_get_cpu_bindings();
        assert( cpu_bindings->count == 2 );
        assert( cpu_bindings->items[0] == 0);
        assert( cpu_bindings->items[1] == 1);

        /* P1 gets 1 core */
        cpu_set_t mask;
        mu_parse_mask("3,7", &mask);
        enable_cpu_set(&spd1->pm, &mask);
        assert( num_omp_threads == 3 );

        cpu_bindings = omptm_omp5_testing__compute_and_get_cpu_bindings();
        assert( cpu_bindings->count == 3 );
        assert( cpu_bindings->items[0] == 0);
        assert( cpu_bindings->items[1] == 1);
        assert( cpu_bindings->items[2] == 3);

        omptm_omp5__finalize();
    }

    /* Finalize */
    assert( DLB_Finalize_sp(spd1) == DLB_SUCCESS );
    assert( DLB_Finalize_sp(spd2) == DLB_SUCCESS );

    /* Test --ompt-thread-manager=omp5 without --lewi/--drom */
    {
        spd1 = DLB_Init_sp(0, &p1_mask, options);
        spd1->options.lewi = false;
        omptm_omp5__init(spd1->id, &spd1->options);
        omptool_parallel_data_t parallel_data = {.level = 1};
        omptm_omp5__parallel_begin(&parallel_data);
        omptm_omp5__into_parallel_function(&parallel_data, 0);
        omptm_omp5__into_parallel_implicit_barrier(&parallel_data);
        omptm_omp5__parallel_end(&parallel_data);
        omptm_omp5__finalize();
        assert( DLB_Finalize_sp(spd1) == DLB_SUCCESS );
    }

    /* Test --ompt-thread-manager=omp5 with an empty mask */
    {
        setenv("OMP_NUM_THREADS", "2", 1);
        unsetenv("OMP_PLACES");
        cpu_set_t empty_mask;
        CPU_ZERO(&empty_mask);
        spd1 = DLB_Init_sp(0, &empty_mask, options);
        omptm_omp5__init(spd1->id, &spd1->options);
        omptm_omp5__finalize();
        assert( DLB_Finalize_sp(spd1) == DLB_SUCCESS );
    }

    return 0;
}
