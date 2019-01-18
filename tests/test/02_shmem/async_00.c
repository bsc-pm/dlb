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

#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"
#include "support/queues.h"
#include "support/debug.h"
#include "support/options.h"

#include <assert.h>
#include <unistd.h>

/* Test message queue */


/* Simple test: register two callbaks and check that each one is called */
static int num_cb_called = 0;
static void cb1_enable_cpu(int cpuid, void *arg) { ++num_cb_called; }
static void cb1_disable_cpu(int cpuid, void *arg) { ++num_cb_called; }
static void test_callbacks(void) {
    pm_interface_t pm = {
        .dlb_callback_enable_cpu_ptr = cb1_enable_cpu,
        .dlb_callback_enable_cpu_arg = NULL,
        .dlb_callback_disable_cpu_ptr = cb1_disable_cpu,
        .dlb_callback_disable_cpu_arg = NULL
    };
    pid_t pid1 = 42;
    cpu_set_t mask = { .__bits = { 0xf } };

    assert( shmem_async_init(pid1, &pm, &mask, NULL) == DLB_SUCCESS );
    shmem_async_enable_cpu(pid1, 1);
    shmem_async_disable_cpu(pid1, 1);
    assert( shmem_async_finalize(pid1) == DLB_SUCCESS );
    assert( num_cb_called == 2 );
}

/* Nested callbacks: send a message to a helper thread that forces to
 * enqueue the same message to himself */
static pid_t pid2;
static void cb2_enable_cpu(int cpuid, void *arg) {
    queue_proc_reqs_t *queue = (queue_proc_reqs_t*) arg;

    /* Simulate a DLB_Lend) -> find_new_guest -> notify_helper  */
    pid_t new_guest;
    queue_proc_reqs_pop(queue, &new_guest);
    if (new_guest > 0) {
        assert( new_guest == pid2 );
        shmem_async_enable_cpu(new_guest, cpuid);
    }
}
static void test_nested_callbacks(void) {
    enum { NUM_REQS = 100 };
    pid2 = 42;
    cpu_set_t mask = { .__bits = { 0xf } };

    /* Set up queue with 10 requests */
    queue_proc_reqs_t queue;
    queue_proc_reqs_init(&queue);
    queue_proc_reqs_push(&queue, pid2, NUM_REQS);

    pm_interface_t pm = {
        .dlb_callback_enable_cpu_ptr = cb2_enable_cpu,
        .dlb_callback_enable_cpu_arg = &queue
    };

    /* Initialize shmem and helper thread */
    assert( shmem_async_init(pid2, &pm, &mask, NULL) == DLB_SUCCESS );

    /* Simulate a DLB_Lend (cpu 0) -> find_new_guest (same pid) -> notify_helper  */
    pid_t new_guest;
    queue_proc_reqs_pop(&queue, &new_guest);
    assert( new_guest == pid2 );
    shmem_async_enable_cpu(pid2, 0);

    /* Wait for the queue to be empty */
    while (queue_proc_reqs_size(&queue) != 0) { usleep(100); }

    /* Finalize shmem and helper */
    assert( shmem_async_finalize(pid2) == DLB_SUCCESS );
}

int main(int argc, char **argv) {
    options_t options;
    options_init(&options, NULL);
    debug_init(&options);

    /* Test that callback are correctly called */
    test_callbacks();

    /* Test message to the same helper */
    test_nested_callbacks();

    return 0;
}
