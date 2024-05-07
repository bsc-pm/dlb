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

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/error.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <pthread.h>
#include <assert.h>

void __gcov_flush() __attribute__((weak));

// Test synchronization between procinfo and cpuinfo when the ownership changes

struct data {
    pthread_barrier_t barrier;
    int initialized;
};

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    pid_t pid = 42;
    cpu_set_t mask;
    mu_parse_mask("0-3", &mask);

    /* Fork process */
    pid_t new_pid = fork();
    if (new_pid >= 0) {
        /* Both parent and child execute concurrently */
        int error;
        struct data *shdata;
        shmem_handler_t *handler = shmem_init((void**)&shdata,
                &(const shmem_props_t) {
                    .size = sizeof(struct data),
                    .name = "test",
                    .key = SHMEM_KEY,
                });
        shmem_lock(handler);
        {
            if (!shdata->initialized) {
                pthread_barrierattr_t attr;
                assert( pthread_barrierattr_init(&attr) == 0 );
                assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
                assert( pthread_barrier_init(&shdata->barrier, &attr, 2) == 0 );
                assert( pthread_barrierattr_destroy(&attr) == 0 );
                shdata->initialized = 1;
            }
        }
        shmem_unlock(handler);

        /* Parent initilizes sub-process */
        if (new_pid > 0) {
            assert( shmem_procinfo__init(pid, 0, &mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
            assert( shmem_cpuinfo__init(pid, 0, &mask, SHMEM_KEY, 0) == DLB_SUCCESS );
        }

        /* Child initilizes external */
        if (new_pid == 0) {
            assert( shmem_procinfo_ext__init(SHMEM_KEY) == DLB_SUCCESS );
        }

        /* Both processes synchronize */
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Child sets new mask */
        if (new_pid == 0) {
            cpu_set_t new_mask;
            mu_parse_mask("0,1", &new_mask);
            assert( shmem_procinfo__setprocessmask(pid, &new_mask, DLB_SYNC_QUERY, NULL)
                    == DLB_SUCCESS );
        }

        /* Parent process loops until new mask is set */
        if (new_pid > 0) {
            int err;
            while ((err = shmem_procinfo__polldrom(pid, NULL, &mask)) == DLB_NOUPDT)
                usleep(1000);
            assert( err == DLB_SUCCESS );
        }

        /* Both processes synchronize */
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Finalize shmem */
        shmem_lock(handler);
        {
            if (shdata->initialized) {
                assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
                shdata->initialized = 0;
            }
        }
        shmem_unlock(handler);
        shmem_finalize(handler, NULL);

        /* Child finalizes and exits */
        if (new_pid == 0) {
            assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
    }

    /* Wait for child process */
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

    /* Update cpuinfo with the new mask */
    shmem_cpuinfo__update_ownership(pid, &mask, NULL);

    /* Get new bindings */
    assert( shmem_cpuinfo__get_thread_binding(pid, 1) == 1 );
    assert( shmem_cpuinfo__get_thread_binding(pid, 0) == 0 );
    assert( shmem_cpuinfo__get_thread_binding(pid, 2) == -1 );
    assert( shmem_cpuinfo__get_thread_binding(pid, 3) == -1 );

    /* Finalize sub-process */
    assert( shmem_cpuinfo__finalize(pid, SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_SUCCESS );

    return 0;
}
