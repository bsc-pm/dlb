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
#include <stdio.h>

/* queue_int */
#define QUEUE_T int
#define QUEUE_SIZE 10
#include "support/queue_template.h"


typedef struct my_struct_t {
    char a;
    double d;
    int i;
} my_struct_t;

/* queue_my_struct_t */
#define QUEUE_T my_struct_t
#define QUEUE_KEY_T char
#define QUEUE_SIZE 10
#include "support/queue_template.h"


int main(int argc, char *argv[]) {

    /* int */
    {
        queue_int queue;
        queue_int_init(&queue);
        assert( queue_int_size(&queue) == 0 );
        assert( queue_int_empty(&queue) == true );
        assert( queue_int_front(&queue) == NULL );
        assert( queue_int_back(&queue) == NULL );

        /* enqueue 42 */
        queue_int_enqueue(&queue, 42);
        assert( queue_int_size(&queue) == 1 );
        assert( queue_int_empty(&queue) == false );
        assert( *(int*)queue_int_front(&queue) == 42 );
        assert( *(int*)queue_int_back(&queue) == 42 );

        /* enqueue 50 */
        queue_int_enqueue(&queue, 50);
        assert( queue_int_size(&queue) == 2 );
        assert( *(int*)queue_int_front(&queue) == 42 );
        assert( *(int*)queue_int_back(&queue) == 50 );

        /* enqueue 60 */
        queue_int_enqueue(&queue, 60);
        assert( queue_int_size(&queue) == 3 );
        assert( *(int*)queue_int_front(&queue) == 42 );
        assert( *(int*)queue_int_back(&queue) == 60 );

        /* dequeue 42 */
        int item;
        queue_int_dequeue(&queue, &item);
        assert( item == 42 );
        assert( queue_int_size(&queue) == 2 );
        assert( *(int*)queue_int_front(&queue) == 50 );
        assert( *(int*)queue_int_back(&queue) == 60 );

        /* loop in linear queue */
        queue_int_enqueue(&queue, 70);
        queue_int_enqueue(&queue, 80);
        queue_int_enqueue(&queue, 90);
        assert( queue.next > queue.front );
        size_t num_iters = 0;
        for (int *it = queue_int_front(&queue);
                it != NULL;
                it = queue_int_next(&queue, it)) {
            printf("item: %d\n", *it);
            ++num_iters;
        }
        assert( num_iters == queue_int_size(&queue) );

        /* remove in linear queue */
        queue_int_remove(&queue, 80);
        assert( *(int*)queue_int_front(&queue) == 50 );
        assert( *(int*)queue_int_back(&queue) == 90 );
        num_iters = 0;
        for (int *it = queue_int_front(&queue);
                it != NULL;
                it = queue_int_next(&queue, it)) {
            printf("item: %d\n", *it);
            ++num_iters;
        }
        assert( num_iters == queue_int_size(&queue) );

        /* loop in circular queue */
        for (int i=0; i<5; ++i) {
            queue_int_enqueue(&queue, i);
            queue_int_dequeue(&queue, &item);
        }
        assert( queue.next < queue.front );
        num_iters = 0;
        for (int *it = queue_int_front(&queue);
                it != NULL;
                it = queue_int_next(&queue, it)) {
            printf("item: %d\n", *it);
            ++num_iters;
        }
        assert( num_iters == queue_int_size(&queue) );

        /* remove in circular queue */
        queue_int_remove(&queue, 2);
        assert( *(int*)queue_int_front(&queue) == 1 );
        assert( *(int*)queue_int_back(&queue) == 4 );
        num_iters = 0;
        for (int *it = queue_int_front(&queue);
                it != NULL;
                it = queue_int_next(&queue, it)) {
            printf("item: %d\n", *it);
            ++num_iters;
        }
        assert( num_iters == queue_int_size(&queue) );

        /* remove in consecutive positions */
        queue_int_enqueue(&queue, 4);
        queue_int_enqueue(&queue, 4);
        queue_int_enqueue(&queue, 4);
        queue_int_enqueue(&queue, 4);
        queue_int_enqueue(&queue, 5);
        assert( queue_int_size(&queue) == 8 );
        queue_int_remove(&queue, 4);
        assert( queue_int_size(&queue) == 3 );

        /* replace integer, should be no-op */
        queue_int_replace(&queue, 5);
        assert( *(int*)queue_int_back(&queue) == 5 );
        assert( queue_int_size(&queue) == 3 );

        queue_int_clear(&queue);
        assert( queue_int_empty(&queue) );
    }

    /* my_struct_t */
    {
        queue_my_struct_t queue;
        queue_my_struct_t_init(&queue);
        assert( queue_my_struct_t_empty(&queue) );

        const my_struct_t struct1 = { .a = 'a', .d = 1.0, .i = 5 };
        const my_struct_t struct2 = { .a = 'A', .d = 3.0, .i = 8 };

        queue_my_struct_t_enqueue(&queue, struct1);
        assert( queue_my_struct_t_size(&queue) == 1 );
        queue_my_struct_t_enqueue(&queue, struct2);
        assert( queue_my_struct_t_size(&queue) == 2 );
        assert( queue_my_struct_t_empty(&queue) == false );

        my_struct_t *front = queue_my_struct_t_front(&queue);
        my_struct_t *back = queue_my_struct_t_back(&queue);
        assert( front->a == 'a'
                && front->d - 1.0 < 1e-9
                && front->i == 5 );
        assert( back->a == 'A'
                && back->d - 3.0 < 1e-9
                && back->i == 8 );

        /* replace */
        const my_struct_t struct3 = { .a = 'A', .d = 4.0, .i = 9 };
        queue_my_struct_t_replace(&queue, struct3);
        assert( queue_my_struct_t_size(&queue) == 2 );
        back = queue_my_struct_t_back(&queue);
        assert( back->a == 'A'
                && back->d - 4.0 < 1e-9
                && back->i == 9 );

        const my_struct_t struct4 = { .a = 'b', .d = 0.0, .i = 0 };
        const my_struct_t struct5 = { .a = 'c', .d = 1.0, .i = 0 };
        const my_struct_t struct6 = { .a = 'd', .d = 2.0, .i = 0 };
        queue_my_struct_t_enqueue(&queue, struct4);
        queue_my_struct_t_enqueue(&queue, struct5);
        queue_my_struct_t_enqueue(&queue, struct6);
        assert( queue_my_struct_t_size(&queue) == 5 );

        /* delete front element */
        queue_my_struct_t_delete(&queue, queue_my_struct_t_front(&queue));
        assert( queue_my_struct_t_size(&queue) == 4 );
        front = queue_my_struct_t_front(&queue);
        assert( front->a == 'A' );

        /* delete middle element */
        for (my_struct_t *it = queue_my_struct_t_front(&queue);
                it != NULL;
                it = queue_my_struct_t_next(&queue, it)) {
            if (it->a == 'c') {
                queue_my_struct_t_delete(&queue, it);
            }
        }
        assert( queue_my_struct_t_size(&queue) == 3 );

        /* delete last element */
        queue_my_struct_t_delete(&queue, queue_my_struct_t_back(&queue));
        assert( queue_my_struct_t_size(&queue) == 2 );
        back = queue_my_struct_t_back(&queue);
        assert( back->a == 'b' );

        for (my_struct_t *it = queue_my_struct_t_front(&queue);
                it != NULL;
                it = queue_my_struct_t_next(&queue, it)) {
            printf("item: %c\n", it->a);
        }

        /* delete middle element in circular buffer */
        for (int i = 0; i < 5; ++i) {
            my_struct_t tmp = { .a = 'z' };;
            queue_my_struct_t_dequeue(&queue, &tmp);
            assert( tmp.a != 'z' );
            ++tmp.a;
            queue_my_struct_t_enqueue(&queue, tmp);
            ++tmp.a;
            queue_my_struct_t_enqueue(&queue, tmp);
        }
        assert( queue_my_struct_t_size(&queue) == 7 );
        assert( queue.front == 6 && queue.next == 3 );
        /* no-op */
        queue_my_struct_t_delete(&queue, queue.items + 3);
        assert( queue_my_struct_t_size(&queue) == 7 );
        queue_my_struct_t_delete(&queue, queue.items + 5);
        assert( queue_my_struct_t_size(&queue) == 7 );
        /* delete */
        queue_my_struct_t_delete(&queue, queue.items);
        assert( queue_my_struct_t_size(&queue) == 6 );
        assert( queue.front == 6 && queue.next == 2 );

        queue_my_struct_t_clear(&queue);
        assert( queue_my_struct_t_empty(&queue) );
    }

    return 0;
}
