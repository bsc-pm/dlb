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

#include "LB_comm/shmem.h"
#include "support/atomic.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

/* Fill node with processes, children will join the shared memory and will test the lock mechanism */

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
    int foo;
};

int main(int argc, char **argv) {

    /* To avoid too many forks on large systems, reduce the system size
     * unless DLB_EXTRA_TESTS is specified. */
    if (!DLB_EXTRA_TESTS) {
        mu_testing_set_sys_size(4);
    }

    int ncpus;
    struct data *shdata;
    shmem_handler_t *handler;

    // Master process creates shmem and initializes barrier
    ncpus = mu_get_system_size();
    handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = SHMEM_KEY,
            });

    // check shdata is aligned to DLB_CACHE_LINE
    assert( (intptr_t)shdata % DLB_CACHE_LINE == 0 );

    fprintf(stdout, "shmem name: %s\n", get_shm_filename(handler));
    pthread_barrierattr_t attr;
    assert( pthread_barrierattr_init(&attr) == 0 );
    assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
    assert( pthread_barrier_init(&shdata->barrier, &attr, ncpus) == 0 );
    assert( pthread_barrierattr_destroy(&attr) == 0 );
    shdata->foo = ncpus;

    // Create a child process per CPU-1
    int child;
    for(child=1; child<ncpus; ++child) {
        pid_t pid = fork();
        assert( pid >= 0 );
        if (pid == 0) {
            // Each child will attach to the shared memory
            handler = shmem_init((void**)&shdata,
                    &(const shmem_props_t) {
                        .size = sizeof(struct data),
                        .name = "test",
                        .key = SHMEM_KEY,
                    });

            // check shdata is aligned to DLB_CACHE_LINE
            assert( (intptr_t)shdata % DLB_CACHE_LINE == 0 );

            pthread_barrier_wait(&shdata->barrier);                             // Barrier 1

            // Everyone performs a locked decrement
            shmem_lock(handler);
            shdata->foo--;
            shmem_unlock(handler);
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 2
            assert(shdata->foo == 1);
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 3

            // Everyone performs a locked increment (using lock maintenance)
            shmem_lock_maintenance(handler);
            shdata->foo++;
            shmem_unlock_maintenance(handler);
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 4
            assert(shdata->foo == ncpus);
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 5

            // Test that shmem cannot be put in BUSY while in MAINTENANCE
            if (child == 1) {
                // PRE-B6: Child 1 sets the maintenance mode
                shmem_lock_maintenance(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 6
                // PRE-B7: Child 1 sets some value
                shdata->foo = 42;
                // PRE-B7: Child 1 unsets the maintenance mode
                shmem_unlock_maintenance(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 7
            } else if (child == 2) {
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 6
                // PRE-B7: Child 2 will read the value in BUSY mode
                shmem_acquire_busy(handler);
                assert(shdata->foo == 42);
                shmem_release_busy(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 7
            } else {
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 6
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 7
            }

            // Test that shmem cannot be put in MAINTENANCE while in BUSY
            if (child == 1) {
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 8
                // PRE-B9: Child 1 wants to acquire in maintenance mode
                shmem_lock_maintenance(handler);
                // PRE-B9: Child 1 will read the value in maintenance mode
                assert(shdata->foo == 42*42);
                shmem_unlock_maintenance(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 9
            } else if (child == 2) {
                // PRE-B8: Child 2 enters in busy mode
                shmem_acquire_busy(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 8
                // PRE-B9: Child 2 sets some value in busy mode
                shdata->foo = 42*42;
                // PRE-B9: Child 2 leaves busy mode
                shmem_release_busy(handler);
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 9
            } else {
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 8
                pthread_barrier_wait(&shdata->barrier);                         // Barrier 9
            }

            // Test that BUSY mode does not cause synchronization
            shmem_acquire_busy(handler);
            // Everybody synchronizes inside
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 10
            if (child == 1) {
                // Only one child deactivates busy mode
                shmem_release_busy(handler);
            }
            pthread_barrier_wait(&shdata->barrier);                             // Barrier 11

            shmem_finalize(handler, NULL);

            // We need to call _exit so that children don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
            break;
        }
    }

    // Master process must do as many barriers as children do
    int i;
    for (i=0; i<11; ++i) {
        pthread_barrier_wait(&shdata->barrier);
    }

    // Finalize everything
    assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
    shmem_finalize(handler, NULL);

    // Wait for all child processes
    int wstatus;
    while(wait(&wstatus) > 0) {
        if (!WIFEXITED(wstatus)) return -1;
    }

    mu_finalize();

    return 0;
}
