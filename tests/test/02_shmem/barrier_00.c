/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem.h"
#include "LB_core/spd.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


/* Test node barrier */

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

void* barrier_fn(void *arg) {
    barrier_t *barrier = arg;
    shmem_barrier__barrier(barrier);
    return NULL;
}

int main(int argc, char **argv) {

    const char *barrier_name = "barrier";
    bool lewi = false;

    /* Test barrier with one process */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();

        printf("Testing barrier with one process\n");
        shmem_barrier__init(SHMEM_KEY);
        assert( shmem_barrier__register(NULL, lewi) == NULL );
        assert( shmem_barrier__find(NULL) == NULL );
        barrier_t *barrier = shmem_barrier__register(barrier_name, lewi);
        assert( barrier != NULL );
        shmem_barrier__barrier(barrier);
        shmem_barrier__print_info(SHMEM_KEY);
        assert( shmem_barrier__attach(barrier) == 2 );
        assert( shmem_barrier__attach(barrier) == 3 );
        assert( shmem_barrier__detach(barrier) == 2 );
        assert( shmem_barrier__detach(barrier) == 1 );
        shmem_barrier__barrier(barrier);

        /* Perform a barrier with two threads of the same process*/
        assert( shmem_barrier__attach(barrier) == 2 );
        pthread_t thread1, thread2;
        pthread_create(&thread1, NULL, barrier_fn, barrier);
        pthread_create(&thread2, NULL, barrier_fn, barrier);
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
        assert( shmem_barrier__detach(barrier) == 1 );

        shmem_barrier__finalize(SHMEM_KEY);
    }

    /* Test that multiple initialize or finalize does not cause errors */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();

        printf("Testing multiple init/finalize\n");
        shmem_barrier__init(SHMEM_KEY);
        barrier_t *barrier = shmem_barrier__register(barrier_name, lewi);
        assert( barrier != NULL );
        barrier_t *barrier_copy = shmem_barrier__find(barrier_name);
        assert( barrier == barrier_copy);
        assert( shmem_barrier__attach(barrier) == 2 );
        shmem_barrier__init(SHMEM_KEY);
        assert( shmem_barrier__attach(barrier) == 3 );
        assert( shmem_barrier__exists() );
        assert( shmem_barrier__detach(barrier) == 2 );
        shmem_barrier__finalize(SHMEM_KEY);
        assert( !shmem_barrier__exists() );
        assert( shmem_barrier__detach(barrier) == DLB_ERR_NOSHMEM );
        shmem_barrier__finalize(SHMEM_KEY);
        shmem_barrier__init(SHMEM_KEY);
        barrier = shmem_barrier__register(barrier_name, lewi);
        assert( barrier != NULL );
        assert( shmem_barrier__exists() );
        assert( shmem_barrier__detach(barrier) == 0 );
        assert( shmem_barrier__detach(barrier) == DLB_ERR_PERM );
        assert( shmem_barrier__attach(barrier) == DLB_ERR_PERM );
        shmem_barrier__barrier(barrier); /* no effect */
        assert( shmem_barrier__find(NULL) == NULL );
        shmem_barrier__finalize(SHMEM_KEY);
        assert( shmem_barrier__detach(barrier) == DLB_ERR_NOSHMEM );
        shmem_barrier__finalize(SHMEM_KEY);
        assert( shmem_barrier__detach(NULL) == DLB_ERR_UNKNOWN );
        assert( !shmem_barrier__exists() );
        assert( shmem_barrier__find(NULL) == NULL );
    }

    /* Test barrier with N processes */
    {
        printf("Testing barrier with N processes\n");
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
                // All childs initialize DLB
                options_t options;
                options_init(&options, NULL);
                debug_init(&options);
                spd_enter_dlb(NULL);
                thread_spd->id = getpid();
                shmem_barrier__init(SHMEM_KEY);
                barrier_t *barrier = shmem_barrier__register(barrier_name, lewi);
                assert( barrier != NULL );

                // Attach to the "test" shared memory and synchronize
                handler = shmem_init((void**)&shdata,
                        &(const shmem_props_t) {
                            .size = sizeof(struct data),
                            .name = "test",
                            .key = SHMEM_KEY,
                        });
                int error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // All processes do multiple DLB_Barriers once they have initialized
                printf("Child %d successfully initialized\n", child);
                shmem_barrier__barrier(barrier);
                shmem_barrier__barrier(barrier);
                shmem_barrier__barrier(barrier);
                printf("Child %d completed all barriers\n", child);

                // Only one process prints shmem info
                if (child == 1) {
                    shmem_barrier__print_info(SHMEM_KEY);
                }

                // Finalize both shared memories
                assert( shmem_barrier__detach(barrier) >= 0 );
                shmem_barrier__finalize(SHMEM_KEY);
                shmem_finalize(handler, NULL);

                // We need to call _exit so that childs don't call assert_shmem destructors,
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
    {
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
                // All childs initialize DLB
                options_t options;
                options_init(&options, NULL);
                debug_init(&options);
                spd_enter_dlb(NULL);
                thread_spd->id = getpid();
                shmem_barrier__init(SHMEM_KEY);
                barrier_t *barrier = shmem_barrier__register(barrier_name, lewi);
                assert( barrier != NULL );

                // Child 1 does DLB_BarrierDetach
                if (child == 1) {
                    assert( shmem_barrier__detach(barrier) >= 0 );
                }

                // Attach to the "test" shared memory and synchronize
                handler = shmem_init((void**)&shdata,
                        &(const shmem_props_t) {
                            .size = sizeof(struct data),
                            .name = "test",
                            .key = SHMEM_KEY,
                        });
                int error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                if (child != 1) {
                    // All other processes do multiple DLB_Barriers once they have initialized
                    shmem_barrier__barrier(barrier);
                    shmem_barrier__barrier(barrier);
                    shmem_barrier__barrier(barrier);
                }

                // Only one process prints shmem info
                if (child == 2) {
                    shmem_barrier__print_info(SHMEM_KEY);
                }

                // Finalize both shared memories
                if (child != 1) {
                    assert( shmem_barrier__detach(barrier) >= 0 );
                }
                shmem_barrier__finalize(SHMEM_KEY);
                shmem_finalize(handler, NULL);

                // We need to call _exit so that childs don't call assert_shmem destructors,
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
