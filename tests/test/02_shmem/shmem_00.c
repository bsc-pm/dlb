/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "assert_noshm.h"

#include "LB_comm/shmem.h"
#include "support/mask_utils.h"

#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
    int foo;
};

int main(int argc, char **argv) {
    int ncpus;
    struct data *shdata;
    shmem_handler_t *handler;

    // Master process creates shmem and initializes barrier
    ncpus = mu_get_system_size();
    handler = shmem_init((void**)&shdata, sizeof(struct data), "cpuinfo", NULL);
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
            // Each child will attach to the shared memory, sync with barrier and finalize
            handler = shmem_init((void**)&shdata, sizeof(struct data), "cpuinfo", NULL);
            int error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);
            shmem_finalize(handler, SHMEM_DELETE);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
            break;
        }
    }

    // Master process syncs with barrier and finalizes everything
    int error = pthread_barrier_wait(&shdata->barrier);
    assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);
    assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
    shmem_finalize(handler, SHMEM_DELETE);

    // Wait for all child processes
    int wstatus;
    while(wait(&wstatus) > 0) {
        if (!WIFEXITED(wstatus)) return -1;
    }
    return 0;
}
