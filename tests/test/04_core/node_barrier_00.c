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

#include "unique_shmem.h"

#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_barrier.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/options.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test node_barrier module */

struct pthread_args {
    subprocess_descriptor_t *spd;
    barrier_t *barrier;
};

void* barrier_fn(void *arg) {
    struct pthread_args *pargs = arg;
    assert( node_barrier(pargs->spd, pargs->barrier) == DLB_SUCCESS );
    return NULL;
}

int main(int argc, char *argv[]) {

    char options[64] = "--barrier --shm-key=";
    strcat(options, SHMEM_KEY);

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    subprocess_descriptor_t spd1 = {.id = 111};
    options_init(&spd1.options, options);
    subprocess_descriptor_t spd2 = {.id = 222};
    options_init(&spd2.options, options);
    shmem_barrier__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
    bool lewi = false;

    /* Register barriers */
    {
        /* Initialize module and default barrier */
        node_barrier_init(&spd1);
        assert( node_barrier(&spd1, NULL) == DLB_SUCCESS );
        assert( node_barrier_attach(&spd1, NULL) == DLB_ERR_PERM );

        int i;
        enum { BARRIER_MAX_LEN = 32 };
        char barrier_name[BARRIER_MAX_LEN];
        uint16_t max_barriers = (uint16_t)shmem_barrier__get_max_barriers();
        uint16_t max_named_barriers = max_barriers - 1;
        assert( max_barriers > 0 );

        if (max_named_barriers > 0) {
            barrier_t **barriers = malloc(sizeof(barrier_t*) * max_named_barriers);

            /* register barrier */
            sprintf(barrier_name, "Barrier 0");
            barriers[0] = node_barrier_register(&spd1, barrier_name, lewi);
            assert( barriers[0] != NULL );
            barrier_t *barrier_copy = node_barrier_register(&spd1, barrier_name, lewi);
            assert( barriers[0] == barrier_copy );
            // make sure two registers don't increase participants
            assert( node_barrier(&spd1, barriers[0]) == DLB_SUCCESS );

            /* attach and detach */
            assert( node_barrier_attach(&spd1, barriers[0]) == DLB_ERR_PERM );
            assert( node_barrier_attach(&spd1, barriers[0]) == DLB_ERR_PERM );
            assert( node_barrier_detach(&spd1, barriers[0]) == 0 );
            assert( node_barrier_detach(&spd1, barriers[0]) == DLB_ERR_PERM );
            assert( node_barrier(&spd1, barriers[0]) == DLB_NOUPDT );

            for (i=0; i<max_named_barriers; ++i) {
                snprintf(barrier_name, BARRIER_MAX_LEN, "Barrier %d", i);
                barriers[i] = node_barrier_register(&spd1, barrier_name, lewi);
                assert( barriers[i] != NULL );
                barrier_t *named_barrier_copy = node_barrier_register(&spd1, barrier_name, lewi);
                assert( barriers[i] == named_barrier_copy );
                assert( node_barrier(&spd1, barriers[i]) == DLB_SUCCESS );
            }
        }

        node_barrier_finalize(&spd1);

        /* Make sure node_barrier_finalize removes all barriers from shared memory */
        assert( shmem_barrier__find(NULL) == NULL );
        assert( shmem_barrier__find("default") == NULL );
        for (i=0; i<max_barriers; ++i) {
            snprintf(barrier_name, BARRIER_MAX_LEN, "Barrier %d", i);
            assert( shmem_barrier__find(barrier_name) == NULL );
        }
    }

    /* Multiple spds */
    {
        node_barrier_init(&spd1);
        node_barrier_init(&spd2);

        /* Two subprocesses register a named barrier */
        const char *test_barrier_name = "Test barrier";
        barrier_t *barrier_spd1 = node_barrier_register(&spd1, test_barrier_name, lewi);
        barrier_t *barrier_spd2 = node_barrier_register(&spd2, test_barrier_name, lewi);
        assert( node_barrier_detach(&spd1, barrier_spd1) == 1 );
        assert( node_barrier_attach(&spd1, barrier_spd1) == 2 );
        assert( node_barrier_detach(&spd2, barrier_spd2) == 1 );
        assert( node_barrier_attach(&spd2, barrier_spd2) == 2 );

        /* Perform a barrier with two threads of the same process*/
        pthread_t thread1, thread2;
        struct pthread_args pargs1 = {.spd = &spd1, .barrier = barrier_spd1 };
        pthread_create(&thread1, NULL, barrier_fn, &pargs1);
        struct pthread_args pargs2 = {.spd = &spd2, .barrier = barrier_spd2 };
        pthread_create(&thread2, NULL, barrier_fn, &pargs2);
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);

        /* Another named barrier */
        const char *test_barrier2_name = "Test barrier 2";
        barrier_t *barrier2_spd1 = node_barrier_register(&spd1, test_barrier2_name, lewi);
        assert( barrier2_spd1 != NULL );
        barrier_t *barrier2_spd2 = node_barrier_register(&spd2, test_barrier2_name, lewi);
        assert( barrier2_spd2 != NULL );
        assert( node_barrier_detach(&spd1, barrier2_spd1) == 1 );
        assert( node_barrier_attach(&spd1, barrier2_spd1) == 2 );
        assert( node_barrier_detach(&spd1, barrier2_spd1) == 1 );
        assert( node_barrier_detach(&spd2, barrier2_spd2) == 0 );

        /* node_barrier on detached barriers */
        assert( node_barrier_detach(&spd1, NULL) == 1 );
        assert( node_barrier(&spd1, NULL) == DLB_NOUPDT );
        assert( node_barrier_detach(&spd1, barrier_spd1) == 1 );
        assert( node_barrier(&spd1, barrier_spd1) == DLB_NOUPDT );

        node_barrier_finalize(&spd1);
        node_barrier_finalize(&spd2);
    }

    shmem_barrier__finalize(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);

    return 0;
}
