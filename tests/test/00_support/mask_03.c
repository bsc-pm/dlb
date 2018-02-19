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

#include "support/mask_utils.h"

#include <sched.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_SIZE 16

static void parse_and_check(const char *str, const int *bits) {
    int i;
    size_t nelems = 0;
    bool error = false;
    cpu_set_t mask;

    mu_parse_mask(str, &mask);

    // check every elem within MAX_SIZE
    for (i=0; i<MAX_SIZE && !error; ++i) {
        error = (bool)CPU_ISSET(i, &mask) != (bool)bits[i];
        nelems += bits[i];
    }

    // check size
    error = error ? error : CPU_COUNT(&mask) != nelems;

    printf("String %s parsed as %s\n", str, mu_to_str(&mask));
    assert( !error );
}

int main( int argc, char **argv ) {
    mu_init();
    mu_testing_set_sys_size(MAX_SIZE);

    parse_and_check("0", (const int[MAX_SIZE]){1, 0, 0});
    parse_and_check("1", (const int[MAX_SIZE]){0, 1, 0});
    parse_and_check("0,2", (const int[MAX_SIZE]){1, 0, 1});

    // Beware the range/bitmask ambiguity
    parse_and_check("101b", (const int[MAX_SIZE]){1, 0, 1});
    parse_and_check("10b", (const int[MAX_SIZE]){1});
    parse_and_check("00000000001b", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    parse_and_check("10", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});

    parse_and_check("1,3-5,6,8", (const int[MAX_SIZE]){0, 1, 0, 1, 1, 1, 1, 0, 1});
    parse_and_check("9-12", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1});

    mu_finalize();
    return EXIT_SUCCESS;
}
