/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include <unistd.h>
#include <assert.h>

/* Notes:
 *  - Queue sizes are actually QUEUE_*_SIZE - 1 because head != tail
 *  - push operations allow pid == 0 but it may cause confusion with enum NOBODY
 */

int main(int argc, char *argv[]) {
    int i;
    pid_t pid;

    enum { SYS_SIZE = 16 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

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

    return 0;
}
