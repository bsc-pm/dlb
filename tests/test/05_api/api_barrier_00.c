/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

#include "extra_tests.h"
#include "unique_shmem.h"

#include <apis/dlb.h>
#include <LB_core/spd.h>
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem.h"
#include "LB_core/spd.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/options.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>


/* Test node barrier */

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
    atomic_int ntimes;
};

static shmem_handler_t* open_shmem(void **shdata) {
    return shmem_init(shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = SHMEM_KEY,
            });
}

/* Callback to count how many times the LeWI module has invoked it */
static int ntimes = 0;
static void cb_count(int num_threads, void *arg) {
    ++ntimes;
}

int main(int argc, char **argv) {

    char options[64] = "--barrier --shm-key=";
    strcat(options, SHMEM_KEY);

    /* Test barrier with one process */
    {
        printf("Testing barrier with one process\n");
        assert( DLB_Init(0, 0, options)             == DLB_SUCCESS );
        assert( DLB_Barrier()                       == DLB_SUCCESS );
        assert( DLB_PrintShmem(0, DLB_COLOR_AUTO)   == DLB_SUCCESS );
        assert( DLB_Finalize()                      == DLB_SUCCESS );

        // other calls after finalize
        assert( DLB_Barrier()                       == DLB_ERR_NOCOMP );
        assert( DLB_BarrierAttach()                 == DLB_ERR_NOCOMP );
        assert( DLB_BarrierDetach()                 == DLB_ERR_NOCOMP );
    }

    /* Test that multiple attach or detach does not cause errors */
    {
        printf("Testing multiple attach/detach\n");
        assert( DLB_Init(0, 0, options)             == DLB_SUCCESS );
        assert( DLB_BarrierAttach()                 == DLB_ERR_PERM );
        assert( shmem_barrier__exists() );
        assert( DLB_BarrierDetach()                 == 0 );
        assert( shmem_barrier__exists() );
        assert( DLB_BarrierDetach()                 == DLB_ERR_PERM );
        assert( DLB_BarrierAttach()                 == 1 );
        assert( shmem_barrier__exists() );
        assert( DLB_BarrierDetach()                 == 0 );
        assert( DLB_BarrierDetach()                 == DLB_ERR_PERM );
        assert( DLB_BarrierDetach()                 == DLB_ERR_PERM );
        assert( DLB_Finalize()                      == DLB_SUCCESS );
        assert( !shmem_barrier__exists() );
    }

    /* Test barrier with N processes */
    if (DLB_EXTRA_TESTS)
    {
        printf("Testing barrier with N processes\n");
        int ncpus;
        struct data *shdata;
        shmem_handler_t *handler;

        // Master process creates shmem and initializes barrier
        ncpus = mu_get_system_size();
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, ncpus-1) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Create a child process per CPU-1
        int child;
        for(child=1; child<ncpus; ++child) {
            pid_t pid = fork();
            assert( pid >= 0 );
            if (pid == 0) {
                // All children initialize DLB, LeWI enabled for this test
                char options_plus_lewi[72];
                snprintf(options_plus_lewi, 72, "--lewi %s", options);
                assert( DLB_Init(0, 0, options_plus_lewi) == DLB_SUCCESS );
                assert( DLB_CallbackSet(dlb_callback_set_num_threads,
                            (dlb_callback_t)cb_count, NULL) == DLB_SUCCESS);

                // Attach to the "test" shared memory and synchronize
                handler = open_shmem((void**)&shdata);
                int error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // All processes do multiple DLB_Barriers once they have initialized
                printf("Child %d successfully initialized\n", child);
                assert( DLB_Barrier() == DLB_SUCCESS );
                assert( DLB_Barrier() == DLB_SUCCESS );
                assert( DLB_Barrier() == DLB_SUCCESS );
                printf("Child %d completed all barriers\n", child);

                // Accumulate the number of times that the LeWI callbacks have
                // been triggered
                DLB_ATOMIC_ADD(&shdata->ntimes, ntimes);
                error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // Check that the barriers have triggered the LeWI callbacks
                assert( shdata->ntimes > 0 );

                // Only one process prints shmem info
                if (child == 1) {
                    assert( DLB_PrintShmem(0, DLB_COLOR_AUTO) == DLB_SUCCESS );
                }

                // Finalize DLB and "test" shared memory
                assert( DLB_Finalize() == DLB_SUCCESS );
                shmem_finalize(handler, NULL);

                // We need to call _exit so that children don't call assert_shmem destructors,
                // but that prevents gcov reports, so we'll call it if defined
                if (__gcov_flush) __gcov_flush();
                _exit(EXIT_SUCCESS);
                break;
            }
        }

        // Wait for all child processes
        int wstatus;
        while(wait(&wstatus) > 0) {
            if (!WIFEXITED(wstatus))
                exit(EXIT_FAILURE);
            int rc = WEXITSTATUS(wstatus);
            if (rc != 0) {
                printf("Child return status: %d\n", rc);
                exit(EXIT_FAILURE);
            }
        }

        // Master process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
    }

    /* Test barrier with N processes where 1 of them detaches from the barrier */
    if (DLB_EXTRA_TESTS)
    {
        printf("Testing barrier with N processes and one of them detaches\n");
        int ncpus;
        struct data *shdata;
        shmem_handler_t *handler;

        // Master process creates shmem and initializes barrier
        ncpus = mu_get_system_size();
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, ncpus-1) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Create a child process per CPU-1
        int child;
        for(child=1; child<ncpus; ++child) {
            pid_t pid = fork();
            assert( pid >= 0 );
            if (pid == 0) {
                // All children initialize DLB
                assert( DLB_Init(0, 0, options) == DLB_SUCCESS );

                // Child 1 does DLB_BarrierDetach
                if (child == 1) {
                    assert( DLB_BarrierDetach() >= 0 );
                }

                // Attach to the "test" shared memory and synchronize
                handler = open_shmem((void**)&shdata);
                int error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                if (child != 1) {
                    // All other processes do multiple DLB_Barriers once they have initialized
                    assert( DLB_Barrier() == DLB_SUCCESS );
                    assert( DLB_Barrier() == DLB_SUCCESS );
                    assert( DLB_Barrier() == DLB_SUCCESS );
                }

                // Only one process prints shmem info
                if (child == 2) {
                    assert( DLB_PrintShmem(0, DLB_COLOR_AUTO) == DLB_SUCCESS );
                }

                // Finalize DLB and "test" shared memory
                assert( DLB_Finalize() == DLB_SUCCESS );
                shmem_finalize(handler, NULL);

                // We need to call _exit so that children don't call assert_shmem destructors,
                // but that prevents gcov reports, so we'll call it if defined
                if (__gcov_flush) __gcov_flush();
                _exit(EXIT_SUCCESS);
                break;
            }
        }

        // Wait for all child processes
        int wstatus;
        while(wait(&wstatus) > 0) {
            if (!WIFEXITED(wstatus))
                exit(EXIT_FAILURE);
            int rc = WEXITSTATUS(wstatus);
            if (rc != 0) {
                printf("Child return status: %d\n", rc);
                exit(EXIT_FAILURE);
            }
        }

        // Master process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
    }

    return EXIT_SUCCESS;
}
