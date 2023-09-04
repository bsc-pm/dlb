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

#include "apis/dlb.h"
#include "apis/dlb_talp.h"
#include "support/env.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem.h"

#include <pthread.h>
#include <string.h>
#include <sys/wait.h>

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

int main(int argc, char *argv[]) {
    char dlb_args[64] = "--talp --talp-external-profiler --shm-key=";
    strcat(dlb_args, SHMEM_KEY);
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_APPEND);

    /* Test DLB_TALP_Attach without any process */
    {
        int ncpus, nelems, pidlist;
        double mpi_time, useful_time;
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetNumCPUs(&ncpus) == DLB_SUCCESS );
        assert( ncpus == mu_get_system_size() );
        assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
        assert( nelems == 0 );
        assert( DLB_TALP_GetTimes(111, &mpi_time, &useful_time) == DLB_ERR_NOENT );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test DLB_TALP_Attach before other processes */
    {
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = shmem_init((void**)&shdata, sizeof(struct data), "test", SHMEM_KEY,
                SHMEM_VERSION_IGNORE, NULL);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, 2) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Parent does TALP_Attach
        int nelems, pidlist;
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
        assert( nelems == 0 );

        // Fork
        pid_t pid = fork();
        assert( pid >= 0 );
        bool child = pid == 0;
        bool parent = !child;

        if (child) {
            assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
            handler = shmem_init((void**)&shdata, sizeof(struct data), "test", SHMEM_KEY,
                    SHMEM_VERSION_IGNORE, NULL);
        }

        int error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (parent) {
            assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
            assert( nelems == 1 && pidlist == pid);
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (child) {
            assert( DLB_Finalize() == DLB_SUCCESS );
            shmem_finalize(handler, NULL);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
        /* if (child) _exit(EXIT_SUCCESS); */

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

        // Parent process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test DLB_TALP_Attach after other process has initialized */
    {
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = shmem_init((void**)&shdata, sizeof(struct data), "test", SHMEM_KEY,
                SHMEM_VERSION_IGNORE, NULL);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, 2) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Fork
        pid_t pid = fork();
        assert( pid >= 0 );
        bool child = pid == 0;
        bool parent = !child;

        if (child) {
            assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
            handler = shmem_init((void**)&shdata, sizeof(struct data), "test", SHMEM_KEY,
                    SHMEM_VERSION_IGNORE, NULL);
        }

        int error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (parent) {
            // Parent does TALP_Attach
            int nelems, pidlist;
            assert( DLB_TALP_Attach() == DLB_SUCCESS );
            assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
            assert( nelems == 1 && pidlist == pid);
            assert( DLB_TALP_Detach() == DLB_SUCCESS );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (child) {
            assert( DLB_Finalize() == DLB_SUCCESS );
            shmem_finalize(handler, NULL);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
        /* if (child) _exit(EXIT_SUCCESS); */

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

        // Parent process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
    }

    return 0;
}
