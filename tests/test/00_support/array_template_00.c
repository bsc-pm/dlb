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

#include <assert.h>

/* array_int */
#define ARRAY_T int
#include "support/array_template.h"


typedef struct my_struct_t {
    char a;
    double d;
    int i;
} my_struct_t;

/* array_my_struct_t */
#define ARRAY_T my_struct_t
#define ARRAY_KEY_T char
#include "support/array_template.h"


int main(int argc, char *argv[]) {

    enum { ARRAY_CAPACITY = 10 };

    /* int */
    {
        array_int array;
        array_int_init(&array, ARRAY_CAPACITY);
        assert( array.count == 0 );
        assert( array.items != NULL );

        array_int_push(&array, 42);
        assert( array.count == 1 );
        array_int_push(&array, 42);
        assert( array.count == 2 );
        array_int_push(&array, 0);
        assert( array.count == 3 );

        assert( array_int_get(&array, 0) == 42 );
        assert( array_int_get(&array, 1) == 42 );
        assert( array_int_get(&array, 2) == 0 );

        array_int_clear(&array);
        assert( array.count == 0 );
        array_int_destroy(&array);
        assert( array.items == NULL );
    }

    /* my_struct_t */
    {
        array_my_struct_t array;
        array_my_struct_t_init(&array, ARRAY_CAPACITY);

        const my_struct_t struct1 = { .a = 'a', .d = 1.0, .i = 5 };
        const my_struct_t struct2 = { .a = 'A', .d = 3.0, .i = 8 };

        array_my_struct_t_push(&array, struct1);
        assert( array.count == 1 );
        array_my_struct_t_push(&array, struct2);
        assert( array.count == 2 );

        assert( array.items[0].a == 'a'
                && array.items[0].d - 1.0 < 1e-9
                && array.items[0].i == 5 );
        assert( array.items[1].a == 'A'
                && array.items[1].d - 3.0 < 1e-9
                && array.items[1].i == 8 );

        my_struct_t item = { .a = 'b', .d = 1.0, .i = 3 };
        array_my_struct_t_push(&array, item);
        item.a = 'r';
        array_my_struct_t_push(&array, item);
        item.a = 'd';
        array_my_struct_t_push(&array, item);
        item.a = 'C';
        array_my_struct_t_push(&array, item);
        array_my_struct_t_sort(&array);
        for (size_t i = 0; i<array.count; ++i) {
            fprintf(stderr, "item %lu: {%c, %f, %d}\n", i,
                    array.items[i].a,
                    array.items[i].d,
                    array.items[i].i);
        }

        array_my_struct_t_clear(&array);
        assert( array.count == 0 );
        array_my_struct_t_destroy(&array);
        assert( array.items == NULL );
    }

    return 0;
}
