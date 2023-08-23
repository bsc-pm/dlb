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
#include "LB_core/DLB_kernel.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/atomic.h"
#include "support/options.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Test node_barrier module integration with LeWI */

struct pthread_args {
    subprocess_descriptor_t *spd;
    barrier_t *barrier;
};

void* barrier_fn(void *arg) {
    struct pthread_args *pargs = arg;
    spd_enter_dlb(pargs->spd);
    assert( node_barrier(pargs->spd, pargs->barrier) == DLB_SUCCESS );
    return NULL;
}

/* Callback to count how many times the LeWI module has invoked it */
static int lewi_ntimes = 0;
static void cb_count(int num_threads, void *arg) {
    ++lewi_ntimes;
}

typedef enum {
    CHECK_NTIMES_ZERO,
    CHECK_NTIMES_POSITIVE,
} lewi_ntimes_check_t;

/* Initialize two spds with the given dlb_flags, perform a barrier with the given
 * barrier and perform a lewi check */
void test_barrier_lewi(const char *dlb_flags, const char *barrier_name,
        int barrier_flags, lewi_ntimes_check_t check) {
    subprocess_descriptor_t spd1;
    subprocess_descriptor_t spd2;
    char options[128];
    sprintf(options, "--barrier --lewi %s --shm-key=%s", dlb_flags, SHMEM_KEY);

    assert( Initialize(&spd1, 111, 0, NULL, options) == DLB_SUCCESS );
    assert( Initialize(&spd2, 222, 0, NULL, options) == DLB_SUCCESS );
    assert( pm_callback_set(&spd1.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)cb_count, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd2.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)cb_count, NULL) == DLB_SUCCESS );

    /* Perform a barrier with two threads of the same process*/
    lewi_ntimes = 0;
    pthread_t thread1, thread2;
    barrier_t *barrier;
    if (barrier_name == NULL) {
        barrier = NULL;
    } else {
        if (shmem_barrier__get_max_barriers() > 1) {
            barrier = node_barrier_register(&spd1, barrier_name, barrier_flags);
            assert( barrier != NULL );
            barrier = node_barrier_register(&spd2, barrier_name, barrier_flags);
            assert( barrier != NULL );
        } else {
            goto finish_dlb;
        }
    }
    struct pthread_args pargs1 = {.spd = &spd1, .barrier = barrier };
    pthread_create(&thread1, NULL, barrier_fn, &pargs1);
    struct pthread_args pargs2 = {.spd = &spd2, .barrier = barrier };
    pthread_create(&thread2, NULL, barrier_fn, &pargs2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    switch(check) {
        case CHECK_NTIMES_ZERO:
            assert( lewi_ntimes == 0 ); break;
        case CHECK_NTIMES_POSITIVE:
            assert( lewi_ntimes > 0 ); break;
    };

finish_dlb:
    assert( Finish(&spd1) == DLB_SUCCESS );
    assert( Finish(&spd2) == DLB_SUCCESS );
}

int main(int argc, char *argv[]) {

    /* Test default barrier with --lewi-barrier */
    test_barrier_lewi("--lewi-barrier=no", NULL, 0, CHECK_NTIMES_ZERO);
    test_barrier_lewi("--lewi-barrier", NULL, 0, CHECK_NTIMES_POSITIVE);

    /* Test default barrier with --lewi-barrier-select overwriting --lewi-barrier */
    test_barrier_lewi("--lewi-barrier=yes --lewi-barrier-select=barrier1,barrier2",
            NULL, 0, CHECK_NTIMES_ZERO);
    test_barrier_lewi("--lewi-barrier=no --lewi-barrier-select=default",
            NULL, 0, CHECK_NTIMES_POSITIVE);
    test_barrier_lewi("--lewi-barrier=no --lewi-barrier-select=barrier1,default",
            NULL, 0, CHECK_NTIMES_POSITIVE);

    /* Test named barrier with different values of --lewi-barrier and API flags */
    test_barrier_lewi("--lewi-barrier=no", "barrier1",
            DLB_BARRIER_LEWI_OFF, CHECK_NTIMES_ZERO);
    test_barrier_lewi("--lewi-barrier=yes", "barrier1",
            DLB_BARRIER_LEWI_OFF, CHECK_NTIMES_ZERO);
    test_barrier_lewi("--lewi-barrier=no", "barrier1",
            DLB_BARRIER_LEWI_ON, CHECK_NTIMES_POSITIVE);
    test_barrier_lewi("--lewi-barrier=yes", "barrier1",
            DLB_BARRIER_LEWI_ON, CHECK_NTIMES_POSITIVE);

    /* Test named barrier with selective flag */
    test_barrier_lewi("--lewi-barrier-select=barrier1", "barrier1",
            DLB_BARRIER_LEWI_OFF, CHECK_NTIMES_ZERO);
    test_barrier_lewi("--lewi-barrier-select=barrier1", "barrier1",
            DLB_BARRIER_LEWI_RUNTIME, CHECK_NTIMES_POSITIVE);
    test_barrier_lewi("--lewi-barrier-select=barrier0,barrier1", "barrier1",
            DLB_BARRIER_LEWI_RUNTIME, CHECK_NTIMES_POSITIVE);
    test_barrier_lewi("", "barrier1",
            DLB_BARRIER_LEWI_RUNTIME, CHECK_NTIMES_ZERO);

    return 0;
}
