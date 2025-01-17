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

/* Test parsing functions */

#include "support/mask_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main(int argc, char *argv[]) {

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

    // Test string size limit
    enum { OVERSIZED_STR_LEN = CPU_SETSIZE * 2 };
    char str[OVERSIZED_STR_LEN];
    for (int i=0; i<OVERSIZED_STR_LEN; ++i) str[i] = '1';
    cpu_set_t mask;
    CPU_ZERO(&mask);
    mu_parse_mask(str, &mask);
    assert( CPU_COUNT(&mask) == 0 );

    /* mu_parse empty mask */
    CPU_SET(1, &mask);
    mu_parse_mask("", &mask);
    assert( CPU_COUNT(&mask) == 0 );

    /* mu_parse_to_slurm_format */
    mu_parse_mask("0-1,4,7,31,63,127-129", &mask);
    char *out_str = mu_parse_to_slurm_format(&mask);
    fprintf(stderr, "Slurm mask: %s\n", out_str);
    assert(strcmp(out_str, "0x380000000000000008000000080000093") == 0);
    free(out_str);

    /* mu_equivalent_masks */
    assert(  mu_equivalent_masks("42", "42") );
    assert(  mu_equivalent_masks("0-7", "0-7") );
    assert( !mu_equivalent_masks("0-7", "0-3") );
    assert( !mu_equivalent_masks("0-7", "0") );

    /* None of the previous tests should initialize mask utils */
    assert( mu_testing_is_initialized() == false );

    return 0;
}
