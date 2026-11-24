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
#define IS_INLINE(name) ((name) == ((__##name).buffer))

/* Returns the expected capacity for a type */
#define EXPECTED_CAP(T) \
    ((SMALL_ARRAY_DEFAULT_BYTES >= sizeof(T)) ? SMALL_ARRAY_DEFAULT_BYTES / sizeof(T) : 1)


static void test_zero_size(void)
{
    SMALL_ARRAY(int, arr, 0);
    assert(arr != NULL);
    assert(__arr.size == 0);
}

static void test_size_one(void)
{
    SMALL_ARRAY(int, arr, 1);
    assert(IS_INLINE(arr));
    arr[0] = 42;
    assert(arr[0] == 42);
}

static void test_int_inline(void)
{
    const size_t N = (SMALL_ARRAY_DEFAULT_BYTES / 2) / sizeof(int);
    SMALL_ARRAY(int, arr, N);

    assert(IS_INLINE(arr) && "int: small array should use inline buffer");
    assert(__arr.size == N);

    for (size_t i = 0; i < N; i++) arr[i] = (int)i;
    for (size_t i = 0; i < N; i++) assert(arr[i] == (int)i);
}

static void test_int_heap(void)
{
    const size_t N = (SMALL_ARRAY_DEFAULT_BYTES * 2) / sizeof(int);
    SMALL_ARRAY(int, arr, N);

    assert(!IS_INLINE(arr) && "int: large array should heap-allocate");
    assert(__arr.size == N);

    for (size_t i = 0; i < N; i++) arr[i] = (int)i;
    for (size_t i = 0; i < N; i++) assert(arr[i] == (int)i);
}

static void test_int_boundary(void)
{
    const size_t N = SMALL_ARRAY_DEFAULT_BYTES / sizeof(int);
    SMALL_ARRAY(int, arr, N);
    assert(IS_INLINE(arr) && "int: boundary size should use inline buffer");
}

static void test_int_just_over_boundary(void)
{
    const size_t N = EXPECTED_CAP(int) + 1;
    SMALL_ARRAY(int, arr, N);
    assert(arr != NULL);
    assert(!IS_INLINE(arr));
    for (size_t i = 0; i < N; i++) arr[i] = (int)i;
    assert(arr[N-1] == (int)(N-1));
}

static void test_cpuid_512(void)
{
    /* By decision, the array in the stack will be able to hold at least 512 CPUs */
    const size_t N = 512;
    SMALL_ARRAY(cpuid_t, map, N);
    assert(IS_INLINE(map));
    assert(__map.size == N);
    for (size_t i = 0; i < N; i++) map[i] = (cpuid_t)i;
    for (size_t i = 0; i < N; i++) assert(map[i] == (cpuid_t)i);
}

int main(void) {

    printf("SMALL_ARRAY_DEFAULT_BYTES = %d\n\n",
           SMALL_ARRAY_DEFAULT_BYTES);

    test_zero_size();
    test_size_one();
    test_int_inline();
    test_int_heap();
    test_int_boundary();
    test_int_just_over_boundary();
    test_cpuid_512();

    return 0;
}
