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
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_SIZE 512

/* Test that the string 'str' is parsed to a cpu_set_t containing the bits in 'bits' */
static void parse_and_check(const char *str, const int *bits) {
    int i;
    int nelems = 0;
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

    fprintf(stderr, "String %s parsed as %s\n", str, mu_to_str(&mask));
    assert( !error );
}

/* Test that the string 'str' is parsed to a cpu_set_t of such 'size' and contains 'cpuid' */
static void parse_and_check2(const char *str, int size, int cpuid) {
    cpu_set_t mask;
    mu_parse_mask(str, &mask);
    fprintf(stderr, "String %s parsed as %s\n", str, mu_to_str(&mask));
    assert( CPU_COUNT(&mask) == size );
    assert( CPU_ISSET(cpuid, &mask) );
}

int main( int argc, char **argv ) {
    mu_init();
    mu_testing_set_sys_size(MAX_SIZE);

    // Decimal
    parse_and_check("0", (const int[MAX_SIZE]){1, 0, 0});
    parse_and_check("1", (const int[MAX_SIZE]){0, 1, 0});
    parse_and_check("0,2", (const int[MAX_SIZE]){1, 0, 1});
    parse_and_check("10", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    // Ranges
    parse_and_check("1,3-5,6,8", (const int[MAX_SIZE]){0, 1, 0, 1, 1, 1, 1, 0, 1});
    parse_and_check("9-12", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1});
    // Long numbers
    parse_and_check2("63", 1, 63);
    parse_and_check2("64", 1, 64);
    parse_and_check2("500-511", 12, 511);

    // Binary
    parse_and_check("0b1", (const int[MAX_SIZE]){1, 0, 0});
    parse_and_check("0b10", (const int[MAX_SIZE]){0, 1, 0});
    parse_and_check("0B101", (const int[MAX_SIZE]){1, 0, 1});
    parse_and_check("0b10000000000", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    parse_and_check("0b00000000000000000000000000000000000000000000000000000000000101111010",
            (const int[MAX_SIZE]){0, 1, 0, 1, 1, 1, 1, 0, 1});
    parse_and_check("0b1111000000000",
            (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1});
    parse_and_check2("0b1000000000000000000000000000000000000000000000000000000000000000", 1, 63);
    parse_and_check2("0b10000000000000000000000000000000000000000000000000000000000000000", 1, 64);
    parse_and_check2("0b"
            "1111111111110000000000000000000000000000000000000000000000000000" /* 64 bits */
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000"
            "0000000000000000000000000000000000000000000000000000000000000000", 12, 511);

    // Hexadecimal
    parse_and_check("0x1", (const int[MAX_SIZE]){1, 0, 0});
    parse_and_check("0x2", (const int[MAX_SIZE]){0, 1, 0});
    parse_and_check("0X5", (const int[MAX_SIZE]){1, 0, 1});
    parse_and_check("0x400", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    parse_and_check("0x17A", (const int[MAX_SIZE]){0, 1, 0, 1, 1, 1, 1, 0, 1});
    parse_and_check("0x00000000000000000000000000000000000000000000000000000000001E00",
            (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1});
    parse_and_check2("0x8000000000000000", 1, 63);
    parse_and_check2("0x10000000000000000", 1, 64);
    parse_and_check2("0x"
            "FFF0000000000000"  /* 64 bits */
            "0000000000000000"
            "0000000000000000"
            "0000000000000000"
            "0000000000000000"
            "0000000000000000"
            "0000000000000000"
            "0000000000000000", 12, 511);

    // Old binary
    parse_and_check("101b", (const int[MAX_SIZE]){1, 0, 1});
    parse_and_check("10b", (const int[MAX_SIZE]){1});
    parse_and_check("00000000001b", (const int[MAX_SIZE]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});


    mu_finalize();
    return EXIT_SUCCESS;
}
