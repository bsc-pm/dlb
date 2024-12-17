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

// Test eachs of shared memory capacity with default size and --shm-size-multiplier

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_lewi_async.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/options.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

/* Every child process calls this function to test the full occupancy
 * of the shared memory. Two barriers are needed, in between the parent
 * process will try to initialize DLB and will get an error */
void init_dlb_wait_and_finalize(const cpu_set_t *system_mask, options_t *options) {

    // Attach to the test shared memory
    struct data *shdata;
    shmem_handler_t *handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = options->shm_key,
            });

    // All children initialize DLB shared memories
    subprocess_descriptor_t child_spd = { .id = getpid() };
    spd_enter_dlb(&child_spd);
    assert( shmem_async_init(child_spd.id, NULL, system_mask, options->shm_key,
                options->shm_size_multiplier) == DLB_SUCCESS );
    assert( shmem_barrier__init(options->shm_key, options->shm_size_multiplier)
            == DLB_SUCCESS );
    assert( shmem_lewi_async__init(child_spd.id, 1, options->shm_key,
                options->shm_size_multiplier) == DLB_SUCCESS );
    assert( shmem_procinfo__init_with_cpu_sharing(child_spd.id, 0, system_mask,
                options->shm_key, options->shm_size_multiplier) == DLB_SUCCESS );
    assert( shmem_talp__init(options->shm_key, options->shm_size_multiplier)
            == DLB_SUCCESS );

    /* Barrier 1 */
    pthread_barrier_wait(&shdata->barrier);

    /* Barrier 2 */
    pthread_barrier_wait(&shdata->barrier);

    // All children finalize DLB shared memories
    unsigned int new_ncpus, nreqs;
    lewi_request_t requests;
    assert( shmem_async_finalize(child_spd.id) == DLB_SUCCESS );
    shmem_barrier__finalize(options->shm_key, options->shm_size_multiplier);
    shmem_lewi_async__finalize(child_spd.id, &new_ncpus, &requests, &nreqs, 0);
    assert( shmem_procinfo__finalize(child_spd.id, false, options->shm_key,
                options->shm_size_multiplier) == DLB_SUCCESS );
    assert( shmem_talp__finalize(child_spd.id) == DLB_SUCCESS );

    // Finalize test shared memory
    shmem_finalize(handler, NULL);
}


int test_shm_multiplier(int size_multiplier, bool try_different_multiplier) {

    cpu_set_t system_mask;
    mu_get_system_mask(&system_mask);
    int system_size = mu_get_system_size();

    int num_children = system_size * size_multiplier;
    int num_participants = num_children + 1;

    /* Specific case: we don't want to fill the shared memories. Instead, the
     * parent process with try to initialize them with a different multiplier */
    if (try_different_multiplier) {
        --num_children;
        --num_participants;
    }

    /* Options and parent spd */
    enum { DLB_ARGS_SIZE = 80 };
    char dlb_args[DLB_ARGS_SIZE];
    snprintf(dlb_args, DLB_ARGS_SIZE, "--shm-size-multiplier=%d --shm-key=%s",
            size_multiplier, SHMEM_KEY);
    options_t options;
    options_init(&options, dlb_args);
    debug_init(&options);
    subprocess_descriptor_t spd = { .id = getpid() };
    spd_enter_dlb(&spd);

    assert( options.shm_size_multiplier == size_multiplier );

    /* Parent process creates shmem and initializes barrier */
    struct data *shdata;
    shmem_handler_t *handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = options.shm_key,
            });
    pthread_barrierattr_t attr;
    assert( pthread_barrierattr_init(&attr) == 0 );
    assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
    assert( pthread_barrier_init(&shdata->barrier, &attr, num_participants) == 0 );
    assert( pthread_barrierattr_destroy(&attr) == 0 );


    /* Create as many child processes as CPUs in the system */
    for (int child = 0; child < num_children; ++child) {
        pid_t pid = fork();
        assert( pid >= 0 );
        if (pid == 0) {

            /* Everybody initialize DLB, wait for the parent, then finalize */
            init_dlb_wait_and_finalize(&system_mask, &options);

            /* We need to call _exit so that children don't call assert_shmem
             * destructors but that prevents gcov reports, so we'll call it if
             * defined */
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
            break;
        }
    }

    /* Barrier 1 */
    pthread_barrier_wait(&shdata->barrier);

    if (!try_different_multiplier) {
        /* With the shared memories full, the parent process cannot attach to any of them:
        * (shared memories for Barrier and TALP do not reserve any space during
        *  init, so they cannot cause a DLB_ERR_NOMEM error) */
        assert( shmem_async_init(spd.id, NULL, &system_mask, options.shm_key,
                    options.shm_size_multiplier) == DLB_ERR_NOMEM );
        assert( shmem_lewi_async__init(spd.id, 1, options.shm_key,
                    options.shm_size_multiplier) == DLB_ERR_NOMEM );
        assert( shmem_procinfo__init_with_cpu_sharing(spd.id, 0, &system_mask,
                    options.shm_key, options.shm_size_multiplier) == DLB_ERR_NOMEM );
    } else {
        /* Specific case: shared memories are not full, check error if using a
         * different multiplier */
        assert( shmem_async_init(spd.id, NULL, &system_mask, options.shm_key,
                    options.shm_size_multiplier * 2) == DLB_ERR_INIT );
        assert( shmem_lewi_async__init(spd.id, 1, options.shm_key,
                    options.shm_size_multiplier * 2) == DLB_ERR_INIT );
        assert( shmem_procinfo__init_with_cpu_sharing(spd.id, 0, &system_mask,
                    options.shm_key, options.shm_size_multiplier * 2) == DLB_ERR_INIT );
    }

    /* Barrier 2 */
    pthread_barrier_wait(&shdata->barrier);

    // Finalize everything
    assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
    shmem_finalize(handler, NULL);

    // Wait for all child processes
    int wstatus;
    while(wait(&wstatus) > 0) {
        if (!WIFEXITED(wstatus)) return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    int error = 0;

    /* Test --shm-size-multiplier flag */
    for (int size_multiplier = 1; size_multiplier <= 5; ++size_multiplier) {
        fprintf(stderr, "Testing with size multiplier: %d\n", size_multiplier);
        error += test_shm_multiplier(size_multiplier, false);
    }

    /* Test error when different values of --shm-size-multiplier */
    fprintf(stderr, "Testing with different size multiplier\n");
    error += test_shm_multiplier(3, true);

    return error;
}
