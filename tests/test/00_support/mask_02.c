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

#include "support/mask_utils.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

enum { MAX_SIZE = 16 };
enum { SYS_SIZE = 8 };


static int check_mask(const cpu_set_t *mask, const int *bits) {
    int i;
    for (i=0; i<MAX_SIZE; ++i) {
        if (CPU_ISSET(i, mask) && bits[i] == 0) return 1;
        if (!CPU_ISSET(i, mask) && bits[i] == 1) return 1;
    }
    return 0;
}

int main( int argc, char **argv ) {

    cpu_set_t mask0, mask1, mask2, mask3;
    CPU_ZERO(&mask0);
    mu_parse_mask("0,1", &mask1);
    mu_parse_mask("0-3", &mask2);
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    assert(  mu_is_subset(&mask0, &mask0) );
    assert(  mu_is_subset(&mask0, &mask1) );
    assert(  mu_is_subset(&mask0, &mask2) );
    assert( !mu_is_subset(&mask1, &mask0) );
    assert(  mu_is_subset(&mask1, &mask1) );
    assert(  mu_is_subset(&mask1, &mask2) );
    assert( !mu_is_subset(&mask2, &mask0) );
    assert( !mu_is_subset(&mask2, &mask1) );
    assert(  mu_is_subset(&mask2, &mask2) );

    assert(  mu_is_superset(&mask0, &mask0) );
    assert( !mu_is_superset(&mask0, &mask1) );
    assert( !mu_is_superset(&mask0, &mask2) );
    assert(  mu_is_superset(&mask1, &mask0) );
    assert(  mu_is_superset(&mask1, &mask1) );
    assert( !mu_is_superset(&mask1, &mask2) );
    assert(  mu_is_superset(&mask2, &mask0) );
    assert(  mu_is_superset(&mask2, &mask1) );
    assert(  mu_is_superset(&mask2, &mask2) );

    assert( !mu_is_proper_subset(&mask0, &mask0) );
    assert(  mu_is_proper_subset(&mask0, &mask1) );
    assert(  mu_is_proper_subset(&mask0, &mask2) );
    assert( !mu_is_proper_subset(&mask1, &mask0) );
    assert( !mu_is_proper_subset(&mask1, &mask1) );
    assert(  mu_is_proper_subset(&mask1, &mask2) );
    assert( !mu_is_proper_subset(&mask2, &mask0) );
    assert( !mu_is_proper_subset(&mask2, &mask1) );
    assert( !mu_is_proper_subset(&mask2, &mask2) );

    assert( !mu_is_proper_superset(&mask0, &mask0) );
    assert( !mu_is_proper_superset(&mask0, &mask1) );
    assert( !mu_is_proper_superset(&mask0, &mask2) );
    assert(  mu_is_proper_superset(&mask1, &mask0) );
    assert( !mu_is_proper_superset(&mask1, &mask1) );
    assert( !mu_is_proper_superset(&mask1, &mask2) );
    assert(  mu_is_proper_superset(&mask2, &mask0) );
    assert(  mu_is_proper_superset(&mask2, &mask1) );
    assert( !mu_is_proper_superset(&mask2, &mask2) );

    assert( !mu_intersects(&mask0, &mask0) );
    assert( !mu_intersects(&mask0, &mask1) );
    assert( !mu_intersects(&mask0, &mask2) );
    assert( !mu_intersects(&mask1, &mask0) );
    assert(  mu_intersects(&mask1, &mask1) );
    assert(  mu_intersects(&mask1, &mask2) );
    assert( !mu_intersects(&mask2, &mask0) );
    assert(  mu_intersects(&mask2, &mask1) );
    assert(  mu_intersects(&mask2, &mask2) );

    mu_substract(&mask3, &mask1, &mask0);
    assert( check_mask(&mask3, (const int[MAX_SIZE]){1, 1, 0, 0}) == 0 );
    mu_substract(&mask3, &mask1, &mask2);
    assert( check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 0, 0}) == 0 );
    mu_substract(&mask3, &mask2, &mask1);
    assert( check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 1, 1}) == 0 );
    mu_substract(&mask3, &mask1, &mask1);
    assert( check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 0, 0}) == 0 );

    assert( CPU_COUNT(&mask1) > 1 );
    assert( mu_get_single_cpu(&mask1) == -1);
    CPU_ZERO(&mask3);
    assert( mu_get_single_cpu(&mask3) == -1);
    CPU_SET(5, &mask3);
    assert( mu_get_single_cpu(&mask3) == 5);

    assert( strcmp(mu_to_str(&mask1), "[0,1]") == 0 );
    assert( strcmp(mu_to_str(&mask2), "[0-3]") == 0 );

    enum { BUFFER_LEN = 16 };
    char buffer[BUFFER_LEN];
    mu_get_quoted_mask(&mask1, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0,1\"") == 0 );
    mu_get_quoted_mask(&mask2, buffer, BUFFER_LEN);
    assert( strcmp(buffer, "\"0-3\"") == 0 );

    mu_finalize();
    return 0;
}
