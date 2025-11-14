/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#include "support/queues.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Notes:
 *  - Queue sizes are actually QUEUE_*_SIZE - 1 because head != tail
 *  - push operations allow pid == 0 but it may cause confusion with enum NOBODY
 */

typedef struct aux_struct_t {
    int a;
    int b;
} aux_struct_t;

#define GENERIC_QUEUE_SIZE 10

struct fold_weighted_sum_accum_t {
    unsigned int step;
    unsigned int sum;
};

void fold_weighted_sum(void *accum, void *curr) {
    struct fold_weighted_sum_accum_t* a = accum;
    a->sum += ((aux_struct_t*)curr)->a * a->step;
    a->step--;
}

struct fold_check_all_int_accum_t {
    int reference;
    bool all_check;
};

void fold_check_all(void *accum, void *curr) {
    struct fold_check_all_int_accum_t* a = accum;
    if (a->all_check) {
        a->all_check = a->reference == *((int*) curr);
    }
}

int main(int argc, char *argv[]) {
    unsigned int i;
    pid_t pid;

    enum { SYS_SIZE = 16 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    /* queue_lewi_reqs_t */
    {
        unsigned int howmany;
        queue_lewi_reqs_t queue;
        queue_lewi_reqs_init(&queue);

        /* Fill queue with requests from different pids */
        howmany = 3;
        for (i=0; i<QUEUE_LEWI_REQS_SIZE-1; ++i) {
            assert( queue_lewi_reqs_size(&queue) == i );
            assert( queue_lewi_reqs_push(&queue, i+1, howmany) == DLB_SUCCESS );
        }
        assert( queue_lewi_reqs_size(&queue) == QUEUE_LEWI_REQS_SIZE-1 );
        assert( queue_lewi_reqs_push(&queue, i+1, howmany) == DLB_ERR_REQST );

        /* Get values */
        for (i=0; i<QUEUE_LEWI_REQS_SIZE-1; ++i) {
            assert( queue_lewi_reqs_get(&queue, i+1) == howmany );
        }

        /* Remove non-existing PID */
        assert( queue_lewi_reqs_remove(&queue, i*2) == 0 );
        assert( queue_lewi_reqs_size(&queue) == QUEUE_LEWI_REQS_SIZE-1 );

        /* Empty queue (forwards until N/2) */
        unsigned int queue_size = QUEUE_LEWI_REQS_SIZE-1;
        for (i=0; i<QUEUE_LEWI_REQS_SIZE/2; ++i) {
            assert( queue_lewi_reqs_remove(&queue, i+1) == howmany );
            assert( queue_lewi_reqs_size(&queue) == --queue_size );
        }

        /* Empty queue (backwards until N/2) */
        for (i=QUEUE_LEWI_REQS_SIZE-2; i>=QUEUE_LEWI_REQS_SIZE/2; --i) {
            assert( queue_lewi_reqs_remove(&queue, i+1) == howmany );
            assert( queue_lewi_reqs_size(&queue) == --queue_size );
        }

        assert( queue_size == 0 );
        assert( queue_lewi_reqs_size(&queue) == 0 );

        /* Remove non-existing PID */
        assert( queue_lewi_reqs_remove(&queue, i*2) == 0 );
        assert( queue_lewi_reqs_size(&queue) == 0 );

        /* PID 0 is not valid */
        assert( queue_lewi_reqs_push(&queue, 0, 1) == DLB_NOUPDT );
        assert( queue_lewi_reqs_size(&queue) == 0 );
        assert( queue_lewi_reqs_remove(&queue, 0) == 0 );
        assert( queue_lewi_reqs_size(&queue) == 0 );

        /* Pushing en existing entry updates the value, or removes if 0 */
        assert( queue_lewi_reqs_push(&queue, 111, 4 ) == DLB_SUCCESS );
        assert( queue_lewi_reqs_size(&queue) == 1 );
        assert( queue_lewi_reqs_push(&queue, 111, 2 ) == DLB_SUCCESS );
        assert( queue_lewi_reqs_size(&queue) == 1 );
        assert( queue_lewi_reqs_push(&queue, 111, 0 ) == DLB_SUCCESS );
        assert( queue_lewi_reqs_size(&queue) == 0 );

        /* Push multiple requests from the same pid, size should be stable */
        pid = 12345;
        howmany = 5;
        for (i=0; i<QUEUE_LEWI_REQS_SIZE*2; ++i) {
            assert( queue_lewi_reqs_push(&queue, pid, howmany) == DLB_SUCCESS );
            assert( queue_lewi_reqs_size(&queue) == 1 );
        }
        assert( queue_lewi_reqs_get(&queue, pid) == howmany*QUEUE_LEWI_REQS_SIZE*2 );
        assert( queue_lewi_reqs_size(&queue) == 1 );
        assert( queue_lewi_reqs_remove(&queue, pid) == howmany*QUEUE_LEWI_REQS_SIZE*2 );
        assert( queue_lewi_reqs_size(&queue) == 0 );

        /* Push request and ask more than it's in the queue */
        {
            lewi_request_t request;
            unsigned int num_requests;
            assert( queue_lewi_reqs_push(&queue, 111, 5) == DLB_SUCCESS );
            assert( queue_lewi_reqs_pop_ncpus(&queue, 42, &request,
                        &num_requests, 1) == 42-5 );
            assert( num_requests == 1 );
            assert( request.pid == 111 && request.howmany == 5);
        }

        /* Push requests with ncpus '3', '5', '1' and pop 6. After that queue should
         * contain [1, 2], and output requests should contain [2, 3, 1] */
        {
            enum { max_requests = 3 };
            lewi_request_t requests[max_requests];
            unsigned int num_requests;
            assert( queue_lewi_reqs_push(&queue, 111, 3) == DLB_SUCCESS );
            assert( queue_lewi_reqs_push(&queue, 222, 5) == DLB_SUCCESS );
            assert( queue_lewi_reqs_push(&queue, 333, 1) == DLB_SUCCESS );
            assert( queue_lewi_reqs_pop_ncpus(&queue, 6, requests, &num_requests,
                        max_requests) == 0 );
            /* Output order is messed but it's not relevant */
            assert( num_requests == 3 );
            assert( requests[0].pid == 333 && requests[0].howmany == 1 );
            assert( requests[1].pid == 111 && requests[1].howmany == 2 );
            assert( requests[2].pid == 222 && requests[2].howmany == 3 );
            /* Remove remaining entries */
            for (i=0; i<3; ++i) {
                assert( queue_lewi_reqs_pop_ncpus(&queue, 1, requests, &num_requests,
                            max_requests) == 0 );
                assert( num_requests == 1 );
                assert( (requests[0].pid == 111 || requests[0].pid == 222)
                        && requests[0].howmany == 1 );
            }
            assert( queue_lewi_reqs_pop_ncpus(&queue, 1, requests, &num_requests,
                        max_requests) == 1 );
            assert( num_requests == 0 );
        }

        /* Try to pop more requests than available */
        {
            enum { max_requests = 3 };
            lewi_request_t requests[max_requests];
            unsigned int num_requests;
            assert( queue_lewi_reqs_size(&queue) == 0 );

            /* With only one element */
            assert( queue_lewi_reqs_push(&queue, 111, 4 ) == DLB_SUCCESS );
            assert( queue_lewi_reqs_pop_ncpus(&queue, 10, requests, &num_requests,
                        max_requests) == 6 );
            assert( num_requests == 1 );
            assert( requests[0].pid == 111 && requests[0].howmany == 4 );
            assert( queue_lewi_reqs_size(&queue) == 0 );

            /* With more than one element */
            assert( queue_lewi_reqs_push(&queue, 111, 4 ) == DLB_SUCCESS );
            assert( queue_lewi_reqs_push(&queue, 222, 5 ) == DLB_SUCCESS );
            assert( queue_lewi_reqs_pop_ncpus(&queue, 10, requests, &num_requests,
                        max_requests) == 1 );
            assert( num_requests == 2 );
            assert( requests[0].pid == 111 && requests[0].howmany == 4 );
            assert( requests[1].pid == 222 && requests[1].howmany == 5 );
            assert( queue_lewi_reqs_size(&queue) == 0 );
        }
    }

    /* queue_proc_reqs_t */
    {
        queue_proc_reqs_t queue;
        queue_proc_reqs_init(&queue);

        /* Fill queue with requests from different pids */
        for (i=0; i<QUEUE_PROC_REQS_SIZE-1; ++i) {
            assert( queue_proc_reqs_size(&queue) == i );
            assert( queue_proc_reqs_push(&queue, i+1, 1, NULL) == DLB_NOTED );
        }
        assert( queue_proc_reqs_size(&queue) == QUEUE_PROC_REQS_SIZE-1 );
        assert( queue_proc_reqs_push(&queue, i+1, 1, NULL) == DLB_ERR_REQST );

        /* Empty queue */
        for (i=0; i<QUEUE_PROC_REQS_SIZE-1; ++i) {
            assert( queue_proc_reqs_size(&queue) == QUEUE_PROC_REQS_SIZE-1-i );
            queue_proc_reqs_get(&queue, &pid, 0);
        }
        assert( queue_proc_reqs_size(&queue) == 0 );

        /* Push multiple requests from the same pid, size should be stable */
        pid = 12345;
        for (i=0; i<QUEUE_PROC_REQS_SIZE*2; ++i) {
            assert( queue_proc_reqs_push(&queue, pid, 5, NULL) == DLB_NOTED );
            assert( queue_proc_reqs_size(&queue) == 1 );
            assert( queue_proc_reqs_front(&queue)->howmany == 5*(i+1) );
        }

        /* Push some new elements and remove the ones in the middle */
        assert( queue_proc_reqs_push(&queue, 111, 40, NULL) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 2 );
        assert( queue_proc_reqs_push(&queue, 222, 43, NULL) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 3 );
        assert( queue_proc_reqs_push(&queue, 111, 40, NULL) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 4 );
        assert( queue_proc_reqs_push(&queue, 333, 53, NULL) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 5 );
        // remove pid 222
        assert( queue.queue[queue.head-3].pid == 222 );
        queue_proc_reqs_remove(&queue, 222);
        assert( queue.queue[queue.head-3].pid == 0 );
        assert( queue_proc_reqs_size(&queue) == 5 );
        // remove pid 111
        assert( queue.queue[queue.head-4].pid == 111 );
        assert( queue.queue[queue.head-2].pid == 111 );
        queue_proc_reqs_remove(&queue, 111);
        assert( queue.queue[queue.head-4].pid == 0 );
        assert( queue.queue[queue.head-2].pid == 0 );
        assert( queue_proc_reqs_size(&queue) == 5 );

        /* Pop all requests from first element to update tail */
        for (i=0; i<QUEUE_PROC_REQS_SIZE*10; ++i) {
            queue_proc_reqs_get(&queue, &pid, 0);
            assert( pid == 12345 );
        }
        assert( queue_proc_reqs_size(&queue) == 1 );
        queue_proc_reqs_get(&queue, &pid, 0);
        assert( pid == 333 );
        assert( queue_proc_reqs_size(&queue) == 1 );

        /* Remove element pushing value 0 */
        assert( queue_proc_reqs_push(&queue, 333, 0, NULL) == DLB_SUCCESS );
        assert( queue_proc_reqs_size(&queue) == 0 );
    }

    /* queue_proc_reqs_t with allowed CPUs */
    {
        cpu_set_t allowed;
        queue_proc_reqs_t queue;
        queue_proc_reqs_init(&queue);

        /* P1 requests 2 CPUs allowing only {0-3} */
        mu_parse_mask("0-3", &allowed);
        assert( queue_proc_reqs_push(&queue, 111, 2, &allowed) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 1 );

        /* Pop CPU 4 is not successful, but CPU 3 is */
        queue_proc_reqs_get(&queue, &pid, 4);
        assert( pid == 0 );
        assert( queue_proc_reqs_size(&queue) == 1 );
        queue_proc_reqs_get(&queue, &pid, 3);
        assert( pid == 111 );
        assert( queue_proc_reqs_size(&queue) == 1 ); /* 1 request pending */
        queue_proc_reqs_get(&queue, &pid, 3);
        assert( pid == 111 );
        assert( queue_proc_reqs_size(&queue) == 0 ); /* 0 requests pending */

        /* Push some requests */
        mu_parse_mask("0-7", &allowed);
        assert( queue_proc_reqs_push(&queue, 111, 2, &allowed) == DLB_NOTED );
        mu_parse_mask("8-15", &allowed);
        assert( queue_proc_reqs_push(&queue, 222, 1, &allowed) == DLB_NOTED );
        mu_parse_mask("0-15", &allowed);
        assert( queue_proc_reqs_push(&queue, 333, 1, &allowed) == DLB_NOTED );
        assert( queue_proc_reqs_size(&queue) == 3 );

        /* Pop requests */
        queue_proc_reqs_get(&queue, &pid, 10);
        assert( pid == 222 );
        assert( queue_proc_reqs_size(&queue) == 3 );
        queue_proc_reqs_get(&queue, &pid, 10);
        assert( pid == 333 );
        assert( queue_proc_reqs_size(&queue) == 3 );
        queue_proc_reqs_get(&queue, &pid, 10);
        assert( pid == 0 );
        assert( queue_proc_reqs_size(&queue) == 3 );
        queue_proc_reqs_get(&queue, &pid, 0);
        assert( pid == 111 );
        assert( queue_proc_reqs_size(&queue) == 3 );
        queue_proc_reqs_get(&queue, &pid, 0);
        assert( pid == 111 );
        assert( queue_proc_reqs_size(&queue) == 0 );
        queue_proc_reqs_get(&queue, &pid, 0);
        assert( pid == 0 );
        assert( queue_proc_reqs_size(&queue) == 0 );
    }

    /* queue_pids_t */
    {
        queue_pids_t queue;
        queue_pids_t aux_queue;
        queue_pids_init(&queue);

        /* Fill queue with requests from different pids */
        for (i=0; i<QUEUE_PIDS_SIZE-1; ++i) {
            assert( queue_pids_size(&queue) == i );
            assert( queue_pids_push(&queue, i+1) == DLB_NOTED );
        }
        assert( queue_pids_size(&queue) == QUEUE_PIDS_SIZE-1 );
        assert( queue_pids_push(&queue, i+1) == DLB_ERR_REQST );

        /* Empty queue */
        for (i=0; i<QUEUE_PIDS_SIZE-1; ++i) {
            assert( queue_pids_size(&queue) == QUEUE_PIDS_SIZE-1-i );
            queue_pids_pop(&queue, &pid);
        }
        assert( queue_pids_size(&queue) == 0 );

        /* Push some new elements and remove the ones in the middle */
        assert( queue_pids_push(&queue, 111) == DLB_NOTED );
        assert( queue_pids_size(&queue) == 1 );
        assert( queue_pids_push(&queue, 222) == DLB_NOTED );
        assert( queue_pids_size(&queue) == 2 );
        assert( queue_pids_push(&queue, 222) == DLB_NOTED );
        assert( queue_pids_size(&queue) == 3 );
        assert( queue_pids_push(&queue, 111) == DLB_NOTED );
        assert( queue_pids_size(&queue) == 4 );
        assert( queue_pids_push(&queue, 333) == DLB_NOTED );
        assert( queue_pids_size(&queue) == 5 );
        memcpy(&aux_queue, &queue, sizeof(queue));
        // remove pid 222
        assert( queue.queue[0] == 222 );
        assert( queue.queue[1] == 222 );
        queue_pids_remove(&queue, 222);
        assert( queue.queue[0] == 0 );
        assert( queue.queue[1] == 0 );
        assert( queue_pids_size(&queue) == 5 );
        // remove pid 111
        assert( queue.queue[2] == 111 );
        assert( queue.queue[7] == 111 );
        queue_pids_remove(&queue, 111);
        assert( queue.queue[2] == 0 );
        assert( queue.queue[7] == 0 );
        assert( queue_pids_size(&queue) == 1 );
        // hack: restore state to remove 111 first
        memcpy(&queue, &aux_queue, sizeof(queue));
        // remove pid 111
        assert( queue.queue[2] == 111 );
        assert( queue.queue[7] == 111 );
        queue_pids_remove(&queue, 111);
        assert( queue.queue[2] == 0 );
        assert( queue.queue[7] == 0 );
        assert( queue_pids_size(&queue) == 4 );
        // remove pid 222
        assert( queue.queue[0] == 222 );
        assert( queue.queue[1] == 222 );
        queue_pids_remove(&queue, 222);
        assert( queue.queue[0] == 0 );
        assert( queue.queue[1] == 0 );
        assert( queue_pids_size(&queue) == 1 );
        // remove pid 333
        assert( queue.queue[3] == 333 );
        queue_pids_remove(&queue, 333);
        assert( queue.queue[3] == 0 );
        assert( queue_pids_size(&queue) == 0 );
    }

    /* queue_t with alloc */
    {
        queue_t *queue = queue__init(sizeof(aux_struct_t), GENERIC_QUEUE_SIZE,
                                     NULL, QUEUE_ALLOC_OVERWRITE);

        /* Fill queue */
        unsigned int queue_capacity = queue__get_capacity(queue);
        for (i=0; i<queue_capacity; i++) {
            assert(queue__get_size(queue) == i);
            assert(queue__get_capacity(queue) == GENERIC_QUEUE_SIZE);

            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            queue__push_head(queue, &element);
        }

        for (i=queue_capacity; i>0; i--) {
            assert(queue__get_size(queue) == i);
            assert(queue__get_capacity(queue) == GENERIC_QUEUE_SIZE);

            aux_struct_t read_element;
            aux_struct_t reff_element = {
                .a = i - 1,
                .b = queue_capacity - i + 1,
            };

            assert( queue__take_head(queue, &read_element) == DLB_SUCCESS );
            assert( 
                read_element.a == reff_element.a &&
                read_element.b == reff_element.b
            );
        }

        aux_struct_t dummy;
        assert( queue__take_head(queue, &dummy) == DLB_NOUPDT );

        queue__destroy(queue);
    }

    /* queue_t with external storage */
    {
        aux_struct_t storage[GENERIC_QUEUE_SIZE];
        queue_t *queue = queue__init(sizeof(aux_struct_t), GENERIC_QUEUE_SIZE, storage, QUEUE_ALLOC_REALLOC);

        // QUEUEU_ALLOC_REALLOC is incompatible with an external storage.
        assert(queue == NULL);
    }
    {
        aux_struct_t storage[GENERIC_QUEUE_SIZE];

        queue_t *queue = queue__init(sizeof(int), GENERIC_QUEUE_SIZE, storage, QUEUE_ALLOC_FIXED);

        // Assert that the queue is empty at the begining
        assert(queue__is_empty(queue));

        int element = 42;
        int element_taken;

        // Assert that the queue is not empty after some data is added
        assert(queue__push_head(queue, &element) != NULL);
        assert(!queue__is_empty(queue));

        // Assert that the queue is empty once the data is taken from the head
        assert(queue__take_head(queue, &element_taken) == DLB_SUCCESS);
        assert(queue__is_empty(queue));
        assert(element == element_taken);

        // Assert that the queue is empty once the data is taken from the tail
        assert(queue__push_head(queue, &element) != NULL);
        assert(!queue__is_empty(queue));

        assert(queue__take_tail(queue, &element_taken) == DLB_SUCCESS);
        assert(queue__is_empty(queue));
        assert(element == element_taken);

        // Check that when we take data into a NULL pointer, data is removed from the queue.
        assert(queue__push_head(queue, &element) != NULL);
        assert(queue__take_head(queue, NULL) == DLB_SUCCESS);
        assert(queue__is_empty(queue));

        assert(queue__push_head(queue, &element) != NULL);
        assert(queue__take_tail(queue, NULL) == DLB_SUCCESS);
        assert(queue__is_empty(queue));

        // Check that when queue is full no more data can be pushed
        assert(queue__is_empty(queue));
        for (i = 0; i < GENERIC_QUEUE_SIZE; i++) {
            assert(queue__push_head(queue, &element) != NULL);
        }
        assert(queue__is_at_capacity(queue));
        assert(queue__push_head(queue, &element) == NULL);
        assert(queue__push_tail(queue, &element) == NULL);

    }
    {
        aux_struct_t storage[GENERIC_QUEUE_SIZE];

        queue_t *queue = queue__init(sizeof(aux_struct_t), GENERIC_QUEUE_SIZE, storage, QUEUE_ALLOC_FIXED);

        /* Fill queue */
        unsigned int queue_capacity = queue__get_capacity(queue);
        queue_capacity -= 1;
        queue_capacity /= 2;

        for (i=0; i<queue_capacity; i++) {
            assert(queue__get_size(queue) == i*2);
            assert(queue__get_capacity(queue) == GENERIC_QUEUE_SIZE);

            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            queue__push_head(queue, &element);
            queue__push_tail(queue, &element);
        }

        for (i=queue_capacity; i>0; i--) {
            assert(queue__get_size(queue) == i*2);
            assert(queue__get_capacity(queue) == GENERIC_QUEUE_SIZE);

            aux_struct_t *peek_element_head, *peek_element_tail;

            aux_struct_t take_element_head, take_element_tail;
            aux_struct_t reff_element = {
                .a = i - 1,
                .b = queue_capacity - i + 1,
            };

            assert( queue__peek_head(queue, (void**) &peek_element_head) == DLB_SUCCESS );
            assert( 
                peek_element_head->a == reff_element.a &&
                peek_element_head->b == reff_element.b
            );

            assert( queue__peek_tail(queue, (void**) &peek_element_tail) == DLB_SUCCESS );
            assert( 
                peek_element_tail->a == reff_element.a &&
                peek_element_tail->b == reff_element.b
            );

            assert( queue__take_head(queue, &take_element_head) == DLB_SUCCESS );
            assert( 
                take_element_head.a == reff_element.a &&
                take_element_head.b == reff_element.b
            );

            assert( queue__take_tail(queue, &take_element_tail) == DLB_SUCCESS );
            assert( 
                take_element_tail.a == reff_element.a &&
                take_element_tail.b == reff_element.b
            );
        }

        aux_struct_t dummy;
        aux_struct_t *dummy_ptr;
        assert( queue__peek_head(queue, (void**) &dummy_ptr) == DLB_NOUPDT );
        assert( queue__take_head(queue, &dummy) == DLB_NOUPDT );
        assert( queue__peek_tail(queue, (void**) &dummy_ptr) == DLB_NOUPDT );
        assert( queue__take_tail(queue, &dummy) == DLB_NOUPDT );

        /*
         * Check full queue when the allocator is QUEUE_ALLOC_FIXED.
         */
        queue_capacity = queue__get_capacity(queue);
        {
            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            aux_struct_t *elem = queue__push_tail(queue, &element);
            assert(elem != NULL);
        }
        for (i=1; i<queue_capacity; i++) {
            assert(queue__get_capacity(queue) == GENERIC_QUEUE_SIZE);

            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            aux_struct_t *elem = queue__push_head(queue, &element);
            assert(elem != NULL);
        }
        {
            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            aux_struct_t *elem = queue__push_tail(queue, &element);
            assert(elem == NULL);
        }

        queue__destroy(queue);
    }

    /* queue_t realloc and iterators */
    {

        queue_t *queue = queue__init(sizeof(aux_struct_t), 0,
                                     NULL, QUEUE_ALLOC_REALLOC);

        /* Fill queue */
        unsigned int queue_capacity = GENERIC_QUEUE_SIZE;

        for (i=0; i<queue_capacity; i++) {
            assert(queue__get_size(queue) == i);

            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            queue__push_head(queue, &element);
        }

        {
            queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);
            struct aux_struct_t * got = queue_iter__get_nth(&iter, 3);
            assert(got->a == 6);
        }

        {
            queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);
            struct fold_weighted_sum_accum_t sum = {
                .step = queue__get_size(queue),
                .sum = 0,
            };
            queue_iter__foreach(&iter, fold_weighted_sum, &sum);
            assert(sum.sum == 330);
            assert(sum.step == 0);
        }

        queue__destroy(queue);
    }
    // Test pushing from the begining and end to make sure unordered reallocation works
    {

        queue_t *queue = queue__init(sizeof(int), 0,
                                     NULL, QUEUE_ALLOC_REALLOC);

        /* Fill queue */
        unsigned int queue_capacity = GENERIC_QUEUE_SIZE/2;

        for (i=0; i <= queue_capacity; i++) {
            int element = queue_capacity - i;
            queue__push_tail(queue, &element);
        }

        for (i=1; i<queue_capacity; i++) {
            int element = queue_capacity + i;
            queue__push_head(queue, &element);
        }

        int check_array[GENERIC_QUEUE_SIZE];
        for (i=0; i<GENERIC_QUEUE_SIZE; i++) {
            check_array[i] = GENERIC_QUEUE_SIZE - i - 1;
        }

        {
            for (i = 0; i < GENERIC_QUEUE_SIZE; i++) {
                queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);
                int* got = queue_iter__get_nth(&iter, i);
                assert(*got == check_array[i]);
            }

            // Check getting value past boud from iterator
            queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);
            int* got = queue_iter__get_nth(&iter, queue__get_size(queue)+42);
            assert(got == NULL);
        }

        queue__destroy(queue);
    }
    {

        queue_t *queue = queue__init(sizeof(int), 0,
                                     NULL, QUEUE_ALLOC_REALLOC);

        /* Fill queue */
        unsigned int queue_capacity = GENERIC_QUEUE_SIZE/2;

        for (i=0; i <= queue_capacity; i++) {
            int element = queue_capacity - i;
            queue__push_tail(queue, &element);
        }

        for (i=1; i<queue_capacity; i++) {
            int element = queue_capacity + i;
            queue__push_head(queue, &element);
        }

        int check_array[GENERIC_QUEUE_SIZE];
        for (i=0; i<GENERIC_QUEUE_SIZE; i++) {
            check_array[i] = GENERIC_QUEUE_SIZE - i - 1;
        }

        {
            for (i = 0; i < GENERIC_QUEUE_SIZE; i++) {
                queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);
                int* got = queue_iter__get_nth(&iter, i);
                assert(*got == check_array[i]);
            }
        }

        queue__destroy(queue);
    }
    /* Check the circular buffere feature */
    {

        queue_t *queue = queue__init(sizeof(int), 2,
                                     NULL, QUEUE_ALLOC_OVERWRITE);

        int element = 42;
        queue__push_head(queue, &element);
        queue__push_head(queue, &element);

        int* element_peeked;
        queue__peek_head(queue, (void*) &element_peeked);
        assert(element == *element_peeked);

        queue__peek_tail(queue, (void*) &element_peeked);
        assert(element == *element_peeked);

        element = 4242;
        queue__push_head(queue, &element);
        queue__push_head(queue, &element);

        queue_iter_head2tail_t iter = queue__into_head2tail_iter(queue);

        struct fold_check_all_int_accum_t check_all = {
            .reference = element,
            .all_check = true,
        };

        queue_iter__foreach(&iter, fold_check_all, &check_all);
        assert(check_all.all_check);
    }

    /* queue_t realloc with head & tail != 0 */
    {

        queue_t *queue = queue__init(sizeof(aux_struct_t), 20,
                                     NULL, QUEUE_ALLOC_OVERWRITE);

        /* Fill queue */
        unsigned int queue_capacity = GENERIC_QUEUE_SIZE;

        for (i=0; i<queue_capacity; i++) {
            assert(queue__get_size(queue) == i);

            aux_struct_t element = {
                .a = i,
                .b = queue_capacity - i,
            };

            if (i % 2) queue__push_head(queue, &element);
            else       queue__push_tail(queue, &element);
        }

        for (i=queue_capacity; i>0; i--) {
            aux_struct_t take_element; 
            aux_struct_t reff_element = {
                .a = i - 1,
                .b = queue_capacity - i + 1,
            };

            if ((i-1) % 2) queue__take_head(queue, &take_element);
            else       queue__take_tail(queue, &take_element);

            assert( 
                take_element.a == reff_element.a &&
                take_element.b == reff_element.b
            );
        }

        queue__destroy(queue);
    }

    mu_finalize();

    return 0;
}
