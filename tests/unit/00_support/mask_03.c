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

/* Test CPU sorting routines */

#include "support/mask_utils.h"

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>

#define _INPUTS_ \
    {0,1,2,3,4,5,6,7}, \
    {1,0,3,2,5,4,7,6}, \
    {7,6,5,4,3,2,1,0}, \
    {6,7,4,5,2,3,0,1}, \
    {4,5,6,7,0,1,2,3}, \
    {5,4,7,6,1,0,3,2}, \
    {0,1,4,5,2,3,6,7}, \
    {1,0,5,4,3,2,7,6}, \
    {0,2,1,4,3,6,7,5}, \
    {7,0,1,2,3,4,5,6}, \
    {3,0,7,6,5,1,2,4}, \
    {4,5,7,6,0,1,2,3}

enum { SYS_SIZE = 8 };

/* just for printing */
int current_topology = -1;

static void setup_topology(int topology) {
    /* Each test is run with these topologies */
    /* 1: [0,1,2,3,4,5,6,7]; no threads per core nor numa nodes
     * 2: [0,4], [1,5], [2,6], [3,7]; 2 threads per core, no numa nodes
     */

    /* Topology 1 */
    enum { T1_SYS_SIZE = SYS_SIZE };

    /* Topology 2 */
    enum { T2_NUM_CORES = 4 };
    enum { T2_NUM_NODES = 1 };
    static cpu_set_t t2_sys_mask = {};
    static cpu_set_t t2_core_masks[T2_NUM_CORES];
    static cpu_set_t t2_node_masks[T2_NUM_NODES];
    if (CPU_COUNT(&t2_sys_mask) == 0) {
        mu_parse_mask("0-7", &t2_sys_mask);
        mu_parse_mask("0,4", &t2_core_masks[0]);
        mu_parse_mask("1,5", &t2_core_masks[1]);
        mu_parse_mask("2,6", &t2_core_masks[2]);
        mu_parse_mask("3,7", &t2_core_masks[3]);
        mu_parse_mask("0-7", &t2_node_masks[0]);
    }

    switch(topology) {
        case 1:
            mu_testing_set_sys_size(T1_SYS_SIZE);
            break;
        case 2:
            mu_testing_set_sys_masks(&t2_sys_mask,
                    t2_core_masks, T2_NUM_CORES,
                    t2_node_masks, T2_NUM_NODES);
            break;
        default:
            abort();
    }

    current_topology = topology;
}

static void check_array(int *array, int n, const int *expected,
        cpu_set_t *process_mask) {
    int i;
    for (i=0; i<n; ++i) {
        if (array[i] != expected[i]) {
            printf("Array comparison failed with mask: %s in topology %d:\n",
                    mu_to_str(process_mask), current_topology);
            int j;
            printf("Got:      "); for (j=0; j<n; ++j) {printf("%d ", array[j]);} printf("\n");
            printf("Expected: "); for (j=0; j<n; ++j) {printf("%d ", expected[j]);} printf("\n");
            exit(1);
        }
    }
}

static void test_sort_by_ownership(int *array, int n, const int *expected,
        cpu_set_t *process_mask) {
    /* Invoke qsort */
    qsort_r(array, n, sizeof(int), mu_cmp_cpuids_by_ownership, process_mask);

    /* Check sorted array */
    check_array(array, n, expected, process_mask);
}

static void test_sort_by_affinity(int *array, int n, const int *expected,
        cpu_set_t affinity[]) {
    /* Invoke qsort */
    qsort_r(array, n, sizeof(int), mu_cmp_cpuids_by_affinity, affinity);

    /* Check sorted array */
    check_array(array, n, expected, affinity);
}

static void test_sort_by_ownership_in_topology(int topology, const int *expected,
        cpu_set_t *process_mask) {

    setup_topology(topology);

    int inputs[][SYS_SIZE] = { _INPUTS_ };
    enum { NUM_INPUTS = sizeof(inputs) / sizeof(inputs[0]) };
    for (int i = 0; i < NUM_INPUTS; ++i) {
        test_sort_by_ownership(inputs[i], SYS_SIZE, expected, process_mask);
    }
}

static void test_sort_by_affinity_in_topology(int topology, const int *expected,
        cpu_set_t affinity[]) {

    setup_topology(topology);

    int inputs[][SYS_SIZE] = { _INPUTS_ };
    enum { NUM_INPUTS = sizeof(inputs) / sizeof(inputs[0]) };
    for (int i = 0; i < NUM_INPUTS; ++i) {
        test_sort_by_affinity(inputs[i], SYS_SIZE, expected, affinity);
    }
}

int main(int argc, char *argv[]) {

    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    /***** Process mask tests *****/
    cpu_set_t process_mask;

    /* Full process mask */
    {
        mu_parse_mask("0xFF", &process_mask);

        int expected_1[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        test_sort_by_ownership_in_topology(1, expected_1, &process_mask);

        int expected_2[SYS_SIZE] = {0,4,1,5,2,6,3,7};
        test_sort_by_ownership_in_topology(2, expected_2, &process_mask);
    }

    /* Process mask is a subset in the firsts CPUs */
    {
        mu_parse_mask("0,4", &process_mask);

        int expected_1[SYS_SIZE] = {0,4,1,2,3,5,6,7};
        test_sort_by_ownership_in_topology(1, expected_1, &process_mask);

        int expected_2[SYS_SIZE] = {0,4,1,5,2,6,3,7};
        test_sort_by_ownership_in_topology(2, expected_2, &process_mask);
    }

    /* Process mask is a subset at the end */
    {
        mu_parse_mask("3,7", &process_mask);

        int expected_1[SYS_SIZE] = {3,7,4,5,6,0,1,2};
        test_sort_by_ownership_in_topology(1, expected_1, &process_mask);

        int expected_2[SYS_SIZE] = {3,7,0,4,1,5,2,6};
        test_sort_by_ownership_in_topology(2, expected_2, &process_mask);
    }

    /* Process mask is a subset in the middle */
    {
        mu_parse_mask("1-2,5-6", &process_mask);

        int expected_1[SYS_SIZE] = {1,2,5,6,3,4,7,0};
        test_sort_by_ownership_in_topology(1, expected_1, &process_mask);

        int expected_2[SYS_SIZE] = {1,5,2,6,3,7,0,4};
        test_sort_by_ownership_in_topology(2, expected_2, &process_mask);
    }

    /***** Affinity tests *****/
    cpu_set_t affinity[4];

    /* Affinity of only one level */
    {
        mu_parse_mask("0-7", &affinity[0]);
        CPU_ZERO(&affinity[1]);

        int expected_1[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        test_sort_by_affinity_in_topology(1, expected_1, affinity);

        int expected_2[SYS_SIZE] = {0,4,1,5,2,6,3,7};
        test_sort_by_affinity_in_topology(2, expected_2, affinity);
    }

    /* Basic affinity of 3 levels */
    {
        mu_parse_mask("0-1", &affinity[0]);
        mu_parse_mask("0-3", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_1[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        test_sort_by_affinity_in_topology(1, expected_1, affinity);

        mu_parse_mask("0,4", &affinity[0]);
        mu_parse_mask("0-1,4-5", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_2[SYS_SIZE] = {0,4,1,5,2,6,3,7};
        test_sort_by_affinity_in_topology(2, expected_2, affinity);
    }

    /* Basic affinity of 3 levels, starting from the end */
    {
        mu_parse_mask("7", &affinity[0]);
        mu_parse_mask("4-7", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_1[SYS_SIZE] = {7,4,5,6,0,1,2,3};
        test_sort_by_affinity_in_topology(1, expected_1, affinity);

        mu_parse_mask("3,7", &affinity[0]);
        mu_parse_mask("2-3,6-7", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_2[SYS_SIZE] = {3,7,2,6,0,4,1,5};
        test_sort_by_affinity_in_topology(2, expected_2, affinity);
    }

    /* Level 0 is in the middle of Level 1 mask */
    {
        mu_parse_mask("5-6", &affinity[0]);
        mu_parse_mask("4-7", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_1[SYS_SIZE] = {5,6,7,4,0,1,2,3};
        test_sort_by_affinity_in_topology(1, expected_1, affinity);

        mu_parse_mask("1,5", &affinity[0]);
        mu_parse_mask("0-2,4-6", &affinity[1]);
        mu_parse_mask("0-7", &affinity[2]);
        CPU_ZERO(&affinity[3]);

        int expected_2[SYS_SIZE] = {1,5,2,6,0,4,3,7};
        test_sort_by_affinity_in_topology(2, expected_2, affinity);
    }

    mu_finalize();
    return 0;
}
