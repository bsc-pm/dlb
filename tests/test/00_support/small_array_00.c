/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#include "support/small_array.h"

#include <stdio.h>
#include <assert.h>

/* Returns 1 if the pointer lives inside the struct's inline buffer. */
#define IS_INLINE(arr_var, struct_var) \
    ((arr_var) == ((__##struct_var).buffer))


static void test_int_inline(void)
{
    size_t N = SMALL_ARRAY_DEFAULT_SIZE / 2;
    SMALL_ARRAY(int, arr, N);

    assert(IS_INLINE(arr, arr) && "int: small array should use inline buffer");
    assert(__arr.size == N);

    for (size_t i = 0; i < N; i++) arr[i] = (int)i * 3;
    for (size_t i = 0; i < N; i++) assert(arr[i] == (int)i * 3);
}

static void test_int_heap(void)
{
    const size_t N = SMALL_ARRAY_DEFAULT_SIZE * 2;
    SMALL_ARRAY(int, arr, N);

    assert(!IS_INLINE(arr, arr) && "int: large array should heap-allocate");
    assert(__arr.size == N);

    for (size_t i = 0; i < N; i++) arr[i] = (int)i;
    for (size_t i = 0; i < N; i++) assert(arr[i] == (int)i);
}

static void test_int_boundary(void)
{
    SMALL_ARRAY(int, arr, SMALL_ARRAY_DEFAULT_SIZE);
    assert(IS_INLINE(arr, arr) && "int: boundary size should use inline buffer");
}

static void test_size_one(void)
{
    SMALL_ARRAY(int, arr, 1);
    assert(IS_INLINE(arr, arr));
    arr[0] = 42;
    assert(arr[0] == 42);
}

int main(void) {

    printf("SMALL_ARRAY_DEFAULT_SIZE = %d\n\n",
           SMALL_ARRAY_DEFAULT_SIZE);

    test_int_inline();
    test_int_heap();
    test_int_boundary();
    test_size_one();

    return 0;
}
