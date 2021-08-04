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

/* Test CPU sorting routines */

#include "support/mask_utils.h"

#include <sched.h>
#include <stdlib.h>

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

static void check_array(int *array, int n, const int *expected,
        cpu_set_t *process_mask) {
    int i;
    for (i=0; i<n; ++i) {
        if (array[i] != expected[i]) {
            printf("Array comparison failed with mask: %s:\n", mu_to_str(process_mask));
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

static void test_sort_by_topology(int *array, int n, const int *expected,
        void *topology) {
    /* Invoke qsort */
    qsort_r(array, n, sizeof(int), mu_cmp_cpuids_by_topology, topology);

    /* Check sorted array */
    check_array(array, n, expected, (cpu_set_t*)topology);
}

int main(int argc, char *argv[]) {

    int i;
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    /***** Process mask tests *****/
    cpu_set_t process_mask;

    /* Full process mask */
    {
        mu_parse_mask("0xFF", &process_mask);
        int expected[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_ownership(inputs[i], SYS_SIZE, expected, &process_mask);
        }
    }

    /* Process mask is a subset in the firsts CPUs */
    {
        mu_parse_mask("0,1", &process_mask);
        int expected[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_ownership(inputs[i], SYS_SIZE, expected, &process_mask);
        }
    }

    /* Process mask is a subset at the end */
    {
        mu_parse_mask("7", &process_mask);
        int expected[SYS_SIZE] = {7,0,1,2,3,4,5,6};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_ownership(inputs[i], SYS_SIZE, expected, &process_mask);
        }
    }

    /* Process mask is a subset in the middle */
    {
        mu_parse_mask("4,5,6", &process_mask);
        int expected[SYS_SIZE] = {4,5,6,7,0,1,2,3};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_ownership(inputs[i], SYS_SIZE, expected, &process_mask);
        }
    }

    /* Process mask is a subset in the middle */
    {
        mu_parse_mask("2,6", &process_mask);
        int expected[SYS_SIZE] = {2,6,3,4,5,7,0,1};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_ownership(inputs[i], SYS_SIZE, expected, &process_mask);
        }
    }

    /***** Topology tests *****/
    cpu_set_t topology[4];

    /* Topology of only one level */
    {
        mu_parse_mask("0-7", &topology[0]);
        CPU_ZERO(&topology[1]);
        int expected[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_topology(inputs[i], SYS_SIZE, expected, &topology);
        }
    }

    /* Basic topology of 3 levels */
    {
        mu_parse_mask("0-1", &topology[0]);
        mu_parse_mask("0-3", &topology[1]);
        mu_parse_mask("0-7", &topology[2]);
        CPU_ZERO(&topology[3]);
        int expected[SYS_SIZE] = {0,1,2,3,4,5,6,7};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_topology(inputs[i], SYS_SIZE, expected, &topology);
        }
    }

    /* Basic topology of 3 levels, starting from the end */
    {
        mu_parse_mask("7", &topology[0]);
        mu_parse_mask("4-7", &topology[1]);
        mu_parse_mask("0-7", &topology[2]);
        CPU_ZERO(&topology[3]);
        int expected[SYS_SIZE] = {7,4,5,6,0,1,2,3};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_topology(inputs[i], SYS_SIZE, expected, &topology);
        }
    }

    /* Level 0 is in the middle of Level 1 mask */
    {
        mu_parse_mask("5-6", &topology[0]);
        mu_parse_mask("4-7", &topology[1]);
        mu_parse_mask("0-7", &topology[2]);
        CPU_ZERO(&topology[3]);
        int expected[SYS_SIZE] = {5,6,7,4,0,1,2,3};
        int inputs[][SYS_SIZE] = { _INPUTS_ };
        enum { num_inputs = sizeof(inputs) / sizeof(inputs[0]) };
        for (i=0; i<num_inputs; ++i) {
            test_sort_by_topology(inputs[i], SYS_SIZE, expected, &topology);
        }
    }

    mu_finalize();
    return 0;
}
