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

/* Test mask utils initialization and system functions */

#include "support/mask_utils.h"
#include "support/debug.h"
#include "support/options.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


int main(int argc, char *argv[]) {

    options_t options = { .verbose = VB_AFFINITY };
    debug_init(&options);

    cpu_set_t system_mask;
    cpu_set_t zero_mask;
    CPU_ZERO(&zero_mask);

    /* Default, usually hwloc */
    {
        warning("===== Default init ======");

        // check it's safe to call init and finalize twice
        mu_init();
        mu_init();
        mu_finalize();
        mu_finalize();
    }

    /* Force no hwloc */
    {
        warning("===== No hwloc init =====");

        mu_testing_init_nohwloc();
        mu_finalize();
    }

    /* Custom size */
    {
        enum { SYS_SIZE = 70 };
        warning("===== Size %d ===== ", SYS_SIZE);

        mu_testing_set_sys_size(SYS_SIZE);
        assert( mu_get_system_size() == SYS_SIZE );
        mu_get_system_mask(&system_mask);
        assert( CPU_COUNT(&system_mask) == SYS_SIZE
                && mu_get_first_cpu(&system_mask) == 0
                && mu_get_last_cpu(&system_mask) == SYS_SIZE - 1 );
        assert( mu_system_has_smt() == false );
        assert( mu_get_system_hwthreads_per_core() == 1 );
        assert( mu_get_cpu_next_core(&system_mask, -2) == -1 );
        assert( mu_get_cpu_next_core(&system_mask, -1) == 0 );
        assert( mu_get_cpu_next_core(&system_mask, 0) == 1 );
        assert( mu_get_cpu_next_core(&system_mask, 68) == 69 );
        assert( mu_get_cpu_next_core(&system_mask, 69) == -1 );
        mu_finalize();
    }

    /* Custom size and topology */
    {
        enum { SYS_NCPUS = 32 };
        enum { SYS_NCORES = 16 };
        enum { SYS_NNODES = 4};
        warning("===== ncpus: %d, ncores: %d, nnodes: %d ===== ",
                SYS_NCPUS, SYS_NCORES, SYS_NNODES);

        mu_testing_set_sys(SYS_NCPUS, SYS_NCORES, SYS_NNODES);
        assert( mu_get_system_size() == SYS_NCPUS );
        mu_get_system_mask(&system_mask);
        assert( CPU_COUNT(&system_mask) == SYS_NCPUS
                && mu_get_first_cpu(&system_mask) == 0
                && mu_get_last_cpu(&system_mask) == SYS_NCPUS - 1 );
        assert( mu_system_has_smt() == true );
        assert( mu_get_system_hwthreads_per_core() == 2 );
        assert( mu_get_cpu_next_core(&system_mask, 0) == 2 );
        assert( mu_get_cpu_next_core(&system_mask, 29) == 30 );
        assert( mu_get_cpu_next_core(&system_mask, 30) == -1 );
        assert( mu_get_cpu_next_core(&system_mask, 31) == -1 );
        assert( mu_get_cpu_next_core(&system_mask, 32) == -1 );

        /* Test core mask functions */
        cpu_set_t mask;
        mu_parse_mask("0-3", &mask);
        assert( mu_count_cores(&mask) == 2 );  // [0,1] [2,3] are counted

        mu_parse_mask("0,1,5,6,10,11", &mask);
        assert( mu_count_cores(&mask) == 2 );  // [0,1] [10,11] are counted

        mu_parse_mask("0-31", &mask);
        assert( mu_count_cores(&mask) == SYS_NCORES ); // all are counted
        assert( mu_get_last_coreid(&mask) == SYS_NCORES-1 );

        int last_cpu;
        while ( -1 != (last_cpu = mu_take_last_coreid(&mask)) ) {
            assert( last_cpu == mu_count_cores(&mask) );
        }

        mu_zero(&mask);

        cpu_set_t mask_correct;
        mu_set_core(&mask, 0);
        mu_parse_mask("0,1", &mask_correct);
        assert( mu_equal(&mask, &mask_correct) );

        mu_set_core(&mask, 3);
        mu_parse_mask("0,1,6,7", &mask_correct);
        assert( mu_equal(&mask, &mask_correct) );

        mu_unset_core(&mask, 0);
        mu_parse_mask("6,7", &mask_correct);
        assert( mu_equal(&mask, &mask_correct) );

        mu_unset_core(&mask, 3);
        mu_zero(&mask_correct);
        assert( mu_equal(&mask, &mask_correct) );

        mu_finalize();
    }

    /* Heterogeneous with custom masks */
    {
        enum { NUM_CORES = 4 };
        enum { NUM_NODES = 2 };
        cpu_set_t core_masks[NUM_CORES];
        cpu_set_t node_masks[NUM_NODES];
        mu_parse_mask("0-1,5-12", &system_mask);
        mu_parse_mask("5-6", &core_masks[0]);
        mu_parse_mask("7-8", &core_masks[1]);
        mu_parse_mask("9-10", &core_masks[2]);
        mu_parse_mask("11-12", &core_masks[3]);
        mu_parse_mask("5-10", &node_masks[0]);
        mu_parse_mask("11-12", &node_masks[1]);
        warning("===== Heterogeneous arch ===== ");

        mu_testing_set_sys_masks(&system_mask, core_masks, NUM_CORES, node_masks, NUM_NODES);
        assert( mu_get_system_size() == 13 );
        assert( mu_system_has_smt() == true );
        assert( mu_get_system_hwthreads_per_core() == 2 );
        assert( mu_get_core_id(0) == -1 );

        /* Test functions that operate on NUMA nodes */
        cpu_set_t affinity_mask;
        mu_get_nodes_intersecting_with_cpuset(&affinity_mask, &system_mask);
        assert( CPU_COUNT(&affinity_mask) == 8
                && mu_get_first_cpu(&affinity_mask) == 5
                && mu_get_last_cpu(&affinity_mask) == 12 );
        mu_get_nodes_subset_of_cpuset(&affinity_mask, &system_mask);
        assert( CPU_COUNT(&affinity_mask) == 8
                && mu_get_first_cpu(&affinity_mask) == 5
                && mu_get_last_cpu(&affinity_mask) == 12 );
        mu_get_nodes_intersecting_with_cpuset(&affinity_mask, &zero_mask);
        assert( CPU_COUNT(&affinity_mask) == 0 );
        mu_get_nodes_subset_of_cpuset(&affinity_mask, &zero_mask);
        assert( CPU_COUNT(&affinity_mask) == 0 );

        /* Test functions that operate on cores */
        cpu_set_t mask;
        mu_parse_mask("6-11", &mask);
        cpu_set_t result;
        mu_get_cores_intersecting_with_cpuset(&result, &mask);
        assert( CPU_COUNT(&result) == 8
                && mu_get_first_cpu(&result) == 5
                && mu_get_last_cpu(&result) == 12 );
        mu_get_cores_subset_of_cpuset(&result, &mask);
        assert( CPU_COUNT(&result) == 4
                && mu_get_first_cpu(&result) == 7
                && mu_get_last_cpu(&result) == 10 );
        mu_get_cores_intersecting_with_cpuset(&result, &zero_mask);
        assert( CPU_COUNT(&result) == 0 );
        mu_get_cores_subset_of_cpuset(&result, &zero_mask);
        assert( CPU_COUNT(&result) == 0 );

        /* mu_get_core_mask */
        assert( mu_get_core_mask(42) == NULL );
        assert( CPU_COUNT_S(8, mu_get_core_mask(12)->set) == 2 );
        assert( mu_get_core_mask(12)->count == 2 );
        assert( mu_get_core_mask(12)->first_cpuid == 11 );
        assert( mu_get_core_mask(12)->last_cpuid == 12 );


        mu_finalize();
    }

    /* Core masks in a round-robin fashion */
    {
        enum { SYS_SIZE = 8 };
        enum { NUM_CORES = 4 };
        enum { NUM_NODES = 1 };
        cpu_set_t core_masks[NUM_CORES];
        cpu_set_t node_masks[NUM_NODES];
        mu_parse_mask("0-7", &system_mask);
        mu_parse_mask("0-7", &node_masks[0]);
        mu_parse_mask("0,4", &core_masks[0]);
        mu_parse_mask("1,5", &core_masks[1]);
        mu_parse_mask("2,6", &core_masks[2]);
        mu_parse_mask("3,7", &core_masks[3]);
        warning("===== Round-robin arch ===== ");

        mu_testing_set_sys_masks(&system_mask, core_masks, NUM_CORES, node_masks, NUM_NODES);
        assert( mu_get_system_size() == SYS_SIZE );
        assert( mu_system_has_smt() == true );
        assert( mu_get_system_hwthreads_per_core() == 2 );
        for (int i = 0; i < SYS_SIZE; ++i) {
            int core_id = i % 4;
            const mu_cpuset_t *core_mask = mu_get_core_mask(i);
            const mu_cpuset_t *core_mask_ = mu_get_core_mask_by_coreid(core_id);
            assert( core_mask_ == core_mask );
            assert( mu_get_core_id(i) == core_id );
            assert( core_mask->count == 2 );
            assert( i == ( i < 4 ? core_mask->first_cpuid : core_mask->last_cpuid ) );
            assert( mu_get_cpu_next_core(&system_mask, i) == ( core_id < 3
                    ? i + 1 : -1 ) );
        }

        cpu_set_t mask;
        mu_parse_mask("0,1,4,5", &mask);
        assert( mu_count_cores(&mask) == 2 );

        mu_parse_mask("0,1,2,3,4,5", &mask);
        assert( mu_count_cores(&mask) == 2 );  // [0,4] [1,5] are counted

        mu_parse_mask("0-7", &mask);
        assert( mu_count_cores(&mask) == NUM_CORES ); // all are counted
        assert( mu_get_last_coreid(&mask) == NUM_CORES-1 );

        int last_cpu;
        while ( -1 != (last_cpu = mu_take_last_coreid(&mask)) ) {
            assert( last_cpu == mu_count_cores(&mask) );
        }
    }

    /* Test operations when system is smaller than mask's setsize */
    {
        warning("===== Other tests ===== ");

        enum { SYS_SIZE = 64 };
        mu_testing_set_sys_size(SYS_SIZE);

        /* Create a mask will garbage at the end */
        cpu_set_t mask;
        mu_parse_mask("0-63", &mask);
        mask.__bits[15] = 1;
        assert( CPU_COUNT(&mask) > 64 );
        assert( CPU_COUNT_S(8, &mask) == 64 );

        /* Ensure that we only iterate valid CPUs */
        int sum = 0;
        for (int cpuid = mu_get_first_cpu(&mask); cpuid >= 0;
                cpuid = mu_get_next_cpu(&mask, cpuid)) {
            sum += cpuid;
            fprintf(stderr, "cpuid: %d\n", cpuid);
        }
        assert( sum == 63*(63+1)/2 ); /* 0 + 1 .. + 63 */

        mu_finalize();
    }

    /* Empty node */
    {
        warning("===== Empty node ===== ");

        enum { SYS_SIZE = 144 };
        enum { NUM_CORES = 4 };
        enum { NUM_NODES = 2 };
        cpu_set_t core_masks[NUM_CORES];
        cpu_set_t node_masks[NUM_NODES];
        mu_parse_mask("0-143", &system_mask);
        mu_parse_mask("28-31,140-143", &node_masks[0]);
        mu_parse_mask("", &node_masks[1]);
        mu_parse_mask("28,140", &core_masks[0]);
        mu_parse_mask("29,141", &core_masks[1]);
        mu_parse_mask("30,142", &core_masks[2]);
        mu_parse_mask("31,143", &core_masks[3]);

        mu_testing_set_sys_masks(&system_mask, core_masks, NUM_CORES, node_masks, NUM_NODES);
    }

    return 0;
}
