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

/* Test basic mask utils functions that do not need to read system's topology,
 * i.e., mostly mask operations */

#include "support/mask_utils.h"

#include <assert.h>
#include <sched.h>
#include <string.h>

int main(int argc, char *argv[]) {

    cpu_set_t mask0, mask1, mask2;
    CPU_ZERO(&mask0);
    mu_parse_mask("0,1", &mask1);
    mu_parse_mask("0-3", &mask2);

    /* mu_is_subset */
    assert(  mu_is_subset(&mask0, &mask0) );
    assert(  mu_is_subset(&mask0, &mask1) );
    assert(  mu_is_subset(&mask0, &mask2) );
    assert( !mu_is_subset(&mask1, &mask0) );
    assert(  mu_is_subset(&mask1, &mask1) );
    assert(  mu_is_subset(&mask1, &mask2) );
    assert( !mu_is_subset(&mask2, &mask0) );
    assert( !mu_is_subset(&mask2, &mask1) );
    assert(  mu_is_subset(&mask2, &mask2) );

    /* mu_is_superset */
    assert(  mu_is_superset(&mask0, &mask0) );
    assert( !mu_is_superset(&mask0, &mask1) );
    assert( !mu_is_superset(&mask0, &mask2) );
    assert(  mu_is_superset(&mask1, &mask0) );
    assert(  mu_is_superset(&mask1, &mask1) );
    assert( !mu_is_superset(&mask1, &mask2) );
    assert(  mu_is_superset(&mask2, &mask0) );
    assert(  mu_is_superset(&mask2, &mask1) );
    assert(  mu_is_superset(&mask2, &mask2) );

    /* mu_is_proper_subset */
    assert( !mu_is_proper_subset(&mask0, &mask0) );
    assert(  mu_is_proper_subset(&mask0, &mask1) );
    assert(  mu_is_proper_subset(&mask0, &mask2) );
    assert( !mu_is_proper_subset(&mask1, &mask0) );
    assert( !mu_is_proper_subset(&mask1, &mask1) );
    assert(  mu_is_proper_subset(&mask1, &mask2) );
    assert( !mu_is_proper_subset(&mask2, &mask0) );
    assert( !mu_is_proper_subset(&mask2, &mask1) );
    assert( !mu_is_proper_subset(&mask2, &mask2) );

    /* mu_is_proper_superset */
    assert( !mu_is_proper_superset(&mask0, &mask0) );
    assert( !mu_is_proper_superset(&mask0, &mask1) );
    assert( !mu_is_proper_superset(&mask0, &mask2) );
    assert(  mu_is_proper_superset(&mask1, &mask0) );
    assert( !mu_is_proper_superset(&mask1, &mask1) );
    assert( !mu_is_proper_superset(&mask1, &mask2) );
    assert(  mu_is_proper_superset(&mask2, &mask0) );
    assert(  mu_is_proper_superset(&mask2, &mask1) );
    assert( !mu_is_proper_superset(&mask2, &mask2) );

    /* mu_intersects */
    assert( !mu_intersects(&mask0, &mask0) );
    assert( !mu_intersects(&mask0, &mask1) );
    assert( !mu_intersects(&mask0, &mask2) );
    assert( !mu_intersects(&mask1, &mask0) );
    assert(  mu_intersects(&mask1, &mask1) );
    assert(  mu_intersects(&mask1, &mask2) );
    assert( !mu_intersects(&mask2, &mask0) );
    assert(  mu_intersects(&mask2, &mask1) );
    assert(  mu_intersects(&mask2, &mask2) );

    /* mu_substract */
    cpu_set_t result;
    mu_substract(&result, &mask1, &mask0);
    assert( CPU_COUNT(&result) == 2 &&
            CPU_ISSET(0, &result) && CPU_ISSET(1, &result) );
    mu_substract(&result, &mask1, &mask2);
    assert( CPU_COUNT(&result) == 0 );
    mu_substract(&result, &mask2, &mask1);
    assert( CPU_COUNT(&result) == 2 &&
            CPU_ISSET(2, &result) && CPU_ISSET(3, &result) );
    mu_substract(&result, &mask1, &mask1);
    assert( CPU_COUNT(&result) == 0 );

    /* mu_and, mu_or, mu_count */
    mu_and(&result, &mask0, &mask1);
    assert( CPU_EQUAL(&result, &mask0) );
    mu_or(&result, &mask0, &mask1);
    assert( CPU_EQUAL(&result, &mask1) );
    assert( mu_count(&result) == CPU_COUNT(&mask1) );

    // warning: from this point mask0 is no longer an empty mask
    cpu_set_t fl_mask, full_mask, empty_mask, mask5, mask200;
    CPU_ZERO(&empty_mask);
    CPU_ZERO(&mask0);
    CPU_ZERO(&mask5);
    CPU_ZERO(&mask200);
    CPU_SET(0, &mask0);
    CPU_SET(5, &mask5);
    CPU_SET(200, &mask200);
    mu_parse_mask("0,1023", &fl_mask);
    mu_parse_mask("0-1023", &full_mask);

    /* mu_get_single_cpu */
    assert( mu_get_single_cpu(&mask0) == 0);
    assert( mu_get_single_cpu(&mask5) == 5);
    assert( mu_get_single_cpu(&mask200) == 200);
    assert( mu_get_single_cpu(&full_mask) == -1);
    assert( mu_get_single_cpu(&empty_mask) == -1);

    /* mu_get_first_cpu */
    assert( mu_get_first_cpu(&mask0) == 0 );
    assert( mu_get_first_cpu(&mask5) == 5 );
    assert( mu_get_first_cpu(&mask200) == 200 );
    assert( mu_get_first_cpu(&full_mask) == 0);
    assert( mu_get_first_cpu(&empty_mask) == -1 );

    /* mu_get_last_cpu */
    assert( mu_get_last_cpu(&mask0) == 0 );
    assert( mu_get_last_cpu(&mask1) == 1 );
    assert( mu_get_last_cpu(&mask200) == 200 );
    assert( mu_get_last_cpu(&full_mask) == 1023);
    assert( mu_get_last_cpu(&empty_mask) == -1 );

    /* mu_get_next_cpu */
    assert( mu_get_next_cpu(&mask0, -2) == -1 );
    assert( mu_get_next_cpu(&mask0, -1) == 0 );
    assert( mu_get_next_cpu(&mask1, 0) == 1 );
    assert( mu_get_next_cpu(&mask1, 1) == -1 );
    CPU_CLR(42, &full_mask);
    assert( mu_get_next_cpu(&full_mask, 41) == 43 );
    assert( mu_get_next_cpu(&full_mask, 42) == 43 );
    CPU_SET(42, &full_mask);
    CPU_CLR(63, &full_mask);
    assert( mu_get_next_cpu(&full_mask, 62) == 64 );
    assert( mu_get_next_cpu(&full_mask, 63) == 64 );
    CPU_SET(63, &full_mask);
    assert( mu_get_next_cpu(&fl_mask, 0) == 1023 );    /* 1st ulong */
    assert( mu_get_next_cpu(&fl_mask, 65) == 1023 );   /* 2nd ulong */
    assert( mu_get_next_cpu(&fl_mask, 130) == 1023 );  /* 3rd ulong */
    assert( mu_get_next_cpu(&fl_mask, 250) == 1023 );  /* 4th ulong */

    /* first, last and next */
    int sum = 0;
    for (int cpuid = mu_get_first_cpu(&full_mask); cpuid >= 0;
            cpuid = mu_get_next_cpu(&full_mask, cpuid)) {
        sum += cpuid;
    }
    assert( sum == 1023*(1023+1)/2 );

    /* mu_get_next_unset */
    assert( mu_get_next_unset(&mask0, -2) == -1 );
    assert( mu_get_next_unset(&mask0, -1) == 1 );
    assert( mu_get_next_unset(&mask0, 0) == 1 );
    assert( mu_get_next_unset(&mask0, 1) == 2 );
    assert( mu_get_next_unset(&mask5, 5) == 6 );
    assert( mu_get_next_unset(&full_mask, 0) == -1 );

    /* mu_to_str */
    assert( strcmp(mu_to_str(&mask0), "[0]") == 0 );
    assert( strcmp(mu_to_str(&mask1), "[0,1]") == 0 );
    assert( strcmp(mu_to_str(&mask2), "[0-3]") == 0 );
    assert( strcmp(mu_to_str(&mask5), "[5]") == 0 );
    assert( strcmp(mu_to_str(&empty_mask), "[]") == 0 );
    assert( strcmp(mu_to_str(&fl_mask), "[0,1023]") == 0 );
    assert( strcmp(mu_to_str(&full_mask), "[0-1023]") == 0 );

    /* mu_get_quoted_mask */
    enum { BUFFER_LEN = 16 };
    char buffer[BUFFER_LEN];
    mu_get_quoted_mask(&mask0, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0\"") == 0 );
    mu_get_quoted_mask(&mask1, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0,1\"") == 0 );
    mu_get_quoted_mask(&mask2, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0-3\"") == 0 );
    mu_get_quoted_mask(&empty_mask, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"\"") == 0 );
    mu_get_quoted_mask(&fl_mask, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0,1023\"") == 0 );
    mu_get_quoted_mask(&full_mask, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0-1023\"") == 0 );

    /* None of the previous tests should initialize mask utils */
    assert( mu_testing_is_initialized() == false );

    return 0;
}
