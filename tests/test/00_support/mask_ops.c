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
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdio.h>
#include "support/mask_utils.h"

#define MAX_SIZE 16

static void fill_mask(cpu_set_t *mask, const int *bits) {
    CPU_ZERO(mask);
    int i;
    for (i=0; i<MAX_SIZE; ++i) {
        if (bits[i]) CPU_SET(i, mask);
    }
}

static int check_mask(const cpu_set_t *mask, const int *bits) {
    int i;
    for (i=0; i<MAX_SIZE; ++i) {
        if (CPU_ISSET(i, mask) && bits[i] == 0) return 1;
        if (!CPU_ISSET(i, mask) && bits[i] == 1) return 1;
    }
    return 0;
}

int main( int argc, char **argv ) {
    int error = 0;

    mu_init();

    cpu_set_t mask0, mask1, mask2, mask3;
    fill_mask(&mask0, (const int[MAX_SIZE]){0, 0, 0, 0});
    fill_mask(&mask1, (const int[MAX_SIZE]){1, 1, 1, 1});
    fill_mask(&mask2, (const int[MAX_SIZE]){1, 1, 0, 0});

    if (!mu_is_subset(&mask0, &mask0)) error++;
    if (!mu_is_subset(&mask0, &mask1)) error++;
    if (!mu_is_subset(&mask0, &mask2)) error++;
    if (!mu_is_subset(&mask2, &mask1)) error++;
    if (mu_is_subset(&mask1, &mask2)) error++;

    mu_substract(&mask3, &mask1, &mask0);
    error += check_mask(&mask3, (const int[MAX_SIZE]){1, 1, 1, 1});
    mu_substract(&mask3, &mask1, &mask2);
    error += check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 1, 1});
    mu_substract(&mask3, &mask2, &mask1);
    error += check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 0, 0});
    mu_substract(&mask3, &mask1, &mask1);
    error += check_mask(&mask3, (const int[MAX_SIZE]){0, 0, 0, 0});

    mu_finalize();
    return error;
}
