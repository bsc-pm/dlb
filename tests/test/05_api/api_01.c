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

#include "apis/dlb.h"
#include "apis/dlb_drom.h"
#include "support/env.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/wait.h>

void __gcov_flush() __attribute__((weak));

/* Test DLB_PreInit */

struct data {
    pthread_barrier_t barrier;
    int initialized;
};

int main(int argc, char **argv) {
    /* Options */
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);

    /* Modify DLB_ARGS to include options */
    dlb_setenv("DLB_ARGS", options, NULL, ENV_APPEND);

    /* Single process */
    {
        /* PreInit with full process mask */
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
        assert( DLB_PreInit(&process_mask, NULL) == DLB_SUCCESS );

        /* Init after binding master thread */
        cpu_set_t current_mask;
        CPU_ZERO(&current_mask);
        CPU_SET(sched_getcpu(), &current_mask);
        sched_setaffinity(0, sizeof(cpu_set_t), &current_mask);
        assert( DLB_Init(0, &current_mask, NULL) == DLB_SUCCESS );

        /* A second Init should return error */
        assert( DLB_Init(0, &current_mask, NULL) == DLB_ERR_INIT );

        /* Check current mask */
        assert( DLB_DROM_Attach() == DLB_SUCCESS );
        assert( DLB_DROM_GetProcessMask(getpid(), &process_mask, 0) == DLB_SUCCESS );
        assert( DLB_DROM_Detach() == DLB_SUCCESS );
        assert( CPU_EQUAL(&process_mask, &current_mask) );

        /* Finalize */
        assert( DLB_Finalize() == DLB_SUCCESS );

        /* A second invocation may be needed to clean up CPUs not being inherited */
        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    /* Test two processes preregistering and two inheriting */
    {
        enum { SYS_SIZE = 4 };
        mu_init();
        mu_testing_set_sys_size(SYS_SIZE);

        /* This process forks to have two parent processes simulating a dlb_run */
        pid_t parent2_pid = fork();
        bool parent1 = parent2_pid > 0;
        bool parent2 = parent2_pid == 0;

        /* parent1 preinializes [0-1], parent2 preinializes [2-3] */
        assert( mu_get_system_size() == 4 );
        cpu_set_t process_mask;
        if (parent1) {
            mu_parse_mask("0-1", &process_mask);
        }
        else if (parent2) {
            mu_parse_mask("2-3", &process_mask);
        }
        assert( DLB_PreInit(&process_mask, NULL) == DLB_SUCCESS );

        /* Both parents fork again into child1 and child2 */
        pid_t child_pid = fork();
        bool child1 = parent1 && child_pid == 0;
        bool child2 = parent2 && child_pid == 0;
        parent1 = parent1 && !child1;
        parent2 = parent2 && !child2;

        /* All processes attach to a helper shared memory and synchronize */
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
                assert( pthread_barrier_init(&shdata->barrier, &attr, 4) == 0 );
                assert( pthread_barrierattr_destroy(&attr) == 0 );
                shdata->initialized = 1;
            }
        }
        shmem_unlock(handler);
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both children initialize */
        if (child1 || child2) {
            CPU_ZERO(&process_mask);
            assert( DLB_Init(0, &process_mask, NULL) == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both children finalize */
        if (child1 || child2) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both parents finalize */
        if (parent1 || parent2) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both children finalize shared memory and exit */
        if (child1 || child2) {
            shmem_finalize(handler, NULL);
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }

        /* parent2 waits for its child, then finalizes shared memory and exits */
        if (parent2) {
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
            shmem_finalize(handler, NULL);
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }

        /* Wait for all child processes */
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

        /* Finalize helper shared memory */
        shmem_lock(handler);
        {
            if (shdata->initialized) {
                assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
                shdata->initialized = 0;
            }
        }
        shmem_unlock(handler);
        shmem_finalize(handler, NULL);
    }

    /* Test two processes preregistering and two inheriting (finalization out of order) */
    {
        enum { SYS_SIZE = 4 };
        mu_init();
        mu_testing_set_sys_size(SYS_SIZE);

        /* This process forks to have two parent processes simulating a dlb_run */
        pid_t parent2_pid = fork();
        bool parent1 = parent2_pid > 0;
        bool parent2 = parent2_pid == 0;

        /* parent1 preinializes [0-1], parent2 preinializes [2-3] */
        assert( mu_get_system_size() == 4 );
        cpu_set_t process_mask;
        if (parent1) {
            mu_parse_mask("0-1", &process_mask);
        }
        else if (parent2) {
            mu_parse_mask("2-3", &process_mask);
        }
        assert( DLB_PreInit(&process_mask, NULL) == DLB_SUCCESS );

        /* Both parents fork again into child1 and child2 */
        pid_t child_pid = fork();
        bool child1 = parent1 && child_pid == 0;
        bool child2 = parent2 && child_pid == 0;
        parent1 = parent1 && !child1;
        parent2 = parent2 && !child2;

        /* All processes attach to a helper shared memory and synchronize */
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
                assert( pthread_barrier_init(&shdata->barrier, &attr, 4) == 0 );
                assert( pthread_barrierattr_destroy(&attr) == 0 );
                shdata->initialized = 1;
            }
        }
        shmem_unlock(handler);
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both children initialize */
        if (child1 || child2) {
            CPU_ZERO(&process_mask);
            assert( DLB_Init(0, &process_mask, NULL) == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Only child2 and parent2 finalize */
        if (child2) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);
        if (parent2) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* child1 and parent1 finalize */
        if (child1) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);
        if (parent1) {
            assert( DLB_Finalize() == DLB_SUCCESS );
        }
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Both children finalize shared memory and exit */
        if (child1 || child2) {
            shmem_finalize(handler, NULL);
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }

        /* parent2 waits for its child, then finalizes shared memory and exits */
        if (parent2) {
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
            shmem_finalize(handler, NULL);
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }

        /* Wait for all child processes */
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

        /* Finalize helper shared memory */
        shmem_lock(handler);
        {
            if (shdata->initialized) {
                assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
                shdata->initialized = 0;
            }
        }
        shmem_unlock(handler);
        shmem_finalize(handler, NULL);
    }

    return 0;
}
