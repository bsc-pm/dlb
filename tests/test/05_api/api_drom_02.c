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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/wait.h>


/* DROM tests of a single process */

struct data {
    pthread_barrier_t barrier;
    int initialized;
};

static shmem_handler_t* open_shmem(void **shdata) {
    return shmem_init(shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = SHMEM_KEY,
            });
}

void __gcov_flush() __attribute__((weak));

void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    cpu_set_t *current_mask = arg;
    memcpy(current_mask, mask, sizeof(cpu_set_t));
}

int main(int argc, char **argv) {
    /* Options */
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);

    mu_init();

    /* Modify DLB_ARGS to include options */
    dlb_setenv("DLB_ARGS", options, NULL, ENV_APPEND);

    pid_t pid = getpid();
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    dlb_drom_flags_t no_flags = DLB_DROM_FLAGS_NONE;

    /* Tests with an empty mask */
    {
        /* Save a copy of DLB_ARGS for further tests, since DROM_Attach adds the pre-init flag */
        char *dlb_args_copy = strdup(getenv("DLB_ARGS"));

        /* Preinitialize with an empty mask */
        cpu_set_t empty_mask;
        CPU_ZERO(&empty_mask);
        assert( DLB_DROM_Attach() == DLB_SUCCESS );
        assert( DLB_DROM_PreInit(pid, &empty_mask, DLB_STEAL_CPUS, NULL) == DLB_SUCCESS );

        /* DLB_Init */
        assert( DLB_Init(0, &empty_mask, NULL) == DLB_SUCCESS );

        /* --drom is automatically set */
        char value[16];
        assert( DLB_GetVariable("--drom", value) == DLB_SUCCESS );
        assert( strcmp(value, "yes") == 0 );

        /* Process mask is still empty */
        cpu_set_t mask;
        assert( DLB_DROM_GetProcessMask(pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_COUNT(&mask) == 0 );

        /* Simulate set process mask from an external process */
        assert( DLB_DROM_SetProcessMask(pid, &process_mask, no_flags) == DLB_SUCCESS );
        assert( DLB_PollDROM(NULL, &mask) == DLB_NOUPDT );  /* own process, already applied */
        assert( DLB_DROM_GetProcessMask(pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &process_mask) );

        assert( DLB_Finalize() == DLB_SUCCESS );
        assert( DLB_DROM_Detach() == DLB_SUCCESS );

        /* Restore DLB_ARGS */
        setenv("DLB_ARGS", dlb_args_copy, 1);
        free(dlb_args_copy);
    }

    /* Test mask setter and getter from own process */
    {
        assert( DLB_Init(0, &process_mask, "--drom") == DLB_SUCCESS );

        /* Get mask */
        cpu_set_t mask;
        assert( DLB_DROM_GetProcessMask(0, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &process_mask) );

        /* Set a new mask with 1 less CPU */
        int i;
        for (i=0; i<CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &mask)) {
                CPU_CLR(i, &mask);
                break;
            }
        }
        assert( DLB_DROM_SetProcessMask(0, &mask, no_flags) == DLB_SUCCESS );
        cpu_set_t new_mask;
        assert( DLB_PollDROM(NULL, &new_mask) == DLB_NOUPDT );
        assert( DLB_DROM_GetProcessMask(0, &new_mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &new_mask) );
        assert( CPU_COUNT(&new_mask) + 1 == CPU_COUNT(&process_mask) );

        /* Set again the original mask with DLB_NO_SYNC */
        assert( DLB_DROM_SetProcessMask(0, &process_mask, DLB_NO_SYNC) == DLB_SUCCESS );
        assert( DLB_PollDROM(NULL, &new_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&process_mask, &new_mask) );

        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    /* Test setprocessmask with SYNC_NOW */
    {
        cpu_set_t current_mask, new_mask1, new_mask2;
        enum { SYS_SIZE = 4 };
        mu_testing_set_sys_size(SYS_SIZE);
        mu_parse_mask("0-3", &process_mask);
        memcpy(&current_mask, &process_mask, sizeof(cpu_set_t));

        /* Fork process */
        pid = fork();
        if (pid >= 0) {
            /* Both parent and child execute concurrently */
            int error;
            struct data *shdata;
            shmem_handler_t *handler = open_shmem((void**)&shdata);
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

            /* Child initializes DLB */
            if (pid == 0) {
                assert( DLB_Init(0, &process_mask, "--drom") == DLB_SUCCESS );
                assert( DLB_CallbackSet(dlb_callback_set_process_mask,
                            (dlb_callback_t)cb_set_process_mask, &current_mask) == DLB_SUCCESS );
            }

            /* Both processes synchronize */
            error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

            mu_parse_mask("0-2", &new_mask1);

            /* Parent process sets mask */
            if (pid > 0) {
                /* Set mask and update with poll drom */
                assert( DLB_DROM_SetProcessMask(pid, &new_mask1, no_flags) == DLB_SUCCESS );

                /* A second update is not yet allowed */
                assert( DLB_DROM_SetProcessMask(pid, &new_mask1, no_flags) == DLB_ERR_PDIRTY );
            }

            /* Both processes synchronize */
            error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

            /* Child process polls */
            if (pid == 0) {
                assert( CPU_EQUAL(&process_mask, &current_mask) );
                assert( DLB_PollDROM_Update() == DLB_SUCCESS );
                assert( !CPU_EQUAL(&process_mask, &current_mask) );
                assert( CPU_EQUAL(&new_mask1, &current_mask) );
            }

            /* Both processes synchronize */
            error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

            mu_parse_mask("0-3", &new_mask2);

            /* Parent process sets mask with SYNC_NOW flag */
            if (pid > 0) {
                assert( DLB_DROM_SetProcessMask(pid, &new_mask2, DLB_SYNC_NOW) == DLB_SUCCESS );
            }

            /* Child process loops until new mask is set */
            if (pid == 0) {
                int err;
                while ((err = DLB_PollDROM_Update()) == DLB_NOUPDT) { usleep(1000); }
                assert( err == DLB_SUCCESS );
                assert( !CPU_EQUAL(&new_mask1, &current_mask) );
                assert( CPU_EQUAL(&new_mask2, &current_mask) );
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

            /* Child finalizes DLB and exits */
            if (pid == 0) {
                assert( DLB_Finalize() == DLB_SUCCESS );
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
    }

    /* Test mask getter and setter without init */
    {
        cpu_set_t mask;
        enum { SYS_SIZE = 4 };
        mu_testing_set_sys_size(SYS_SIZE);

        /* Test that error is NOPROC instead of NOSHMEM */
        assert( DLB_DROM_GetProcessMask(0, &mask, no_flags) == DLB_ERR_NOPROC );
        assert( DLB_DROM_SetProcessMask(0, &mask, no_flags) == DLB_ERR_NOPROC );

        /* Fork process */
        pid = fork();
        if (pid >= 0) {
            /* Both parent and child execute concurrently */
            int error;
            struct data *shdata;
            shmem_handler_t *handler = open_shmem((void**)&shdata);
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

            /* Child initializes DLB */
            if (pid == 0) {
                mu_parse_mask("0-3", &process_mask);
                assert( DLB_Init(0, &process_mask, "--drom") == DLB_SUCCESS );
            }

            /* Both processes synchronize */
            error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

            /* Parent process gets the mask without DROM_Attach */
            if (pid != 0) {
                cpu_set_t child_mask, expected_child_mask;
                mu_parse_mask("0-3", &expected_child_mask);
                assert( DLB_DROM_GetProcessMask(pid, &child_mask, no_flags) == DLB_SUCCESS );
                assert( CPU_EQUAL(&child_mask, &expected_child_mask) );
            }

            /* Parent process sets a new mask without DROM_Attach */
            if (pid != 0) {
                cpu_set_t new_mask;
                mu_parse_mask("0-2", &new_mask);
                assert( DLB_DROM_SetProcessMask(pid, &new_mask, no_flags) == DLB_SUCCESS );
            }

            /* Both processes synchronize */
            error = pthread_barrier_wait(&shdata->barrier);
            assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

            /* Child process updates new mask */
            if (pid == 0) {
                cpu_set_t new_mask, expected_new_mask;
                mu_parse_mask("0-2", &expected_new_mask);
                assert( DLB_DROM_GetProcessMask(0, &new_mask, no_flags) == DLB_NOTED );
                assert( CPU_EQUAL(&new_mask, &expected_new_mask) );
            }

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

            /* Child finalizes DLB and exits */
            if (pid == 0) {
                assert( DLB_Finalize() == DLB_SUCCESS );
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
    }

    /* Test child processes inheriting parent preinit */
    {
        cpu_set_t mask;
        enum { SYS_SIZE = 4 };
        mu_testing_set_sys_size(SYS_SIZE);

        /* Parent process preinitializes */
        mu_parse_mask("0-3", &mask);
        assert( DLB_DROM_Attach() == DLB_SUCCESS );
        assert( DLB_DROM_PreInit(pid, &mask, no_flags, NULL) == DLB_SUCCESS );

        /* Fork processes */
        pid_t child_pid[SYS_SIZE];
        int i;
        for(i=0; i<SYS_SIZE; ++i) {
            child_pid[i] = fork();
            if (child_pid[i] == 0) {
                /* Child initializes test shmem */
                int error;
                struct data *shdata;
                shmem_handler_t *handler = open_shmem((void**)&shdata);
                shmem_lock(handler);
                {
                    if (!shdata->initialized) {
                        pthread_barrierattr_t attr;
                        assert( pthread_barrierattr_init(&attr) == 0 );
                        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
                        assert( pthread_barrier_init(&shdata->barrier, &attr, SYS_SIZE+1) == 0 );
                        assert( pthread_barrierattr_destroy(&attr) == 0 );
                        shdata->initialized = 1;
                    }
                }
                shmem_unlock(handler);

                /* Initialize DLB */
                CPU_ZERO(&process_mask);
                CPU_SET(i, &process_mask);
                /* assert( DLB_Init(0, &process_mask, "--drom") == DLB_NOTED ); */
                int err = DLB_Init(0, &process_mask, "--drom");
                fprintf(stderr, "DLB_Init err: %d\n", err);

                /* Synchronize: all children are initialized */
                error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                /* Parent process checks everything is correct */

                /* Synchronize: all children will finalize */
                error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                /* Finalize shmem */
                shmem_finalize(handler, NULL);

                /* Finalizes DLB and exit */
                assert( DLB_Finalize() == DLB_SUCCESS );
                if (__gcov_flush) __gcov_flush();
                _exit(EXIT_SUCCESS);
            }
        }

        /* Parent process initializes test shmem */
        int error;
        struct data *shdata;
        shmem_handler_t *handler = open_shmem((void**)&shdata);
        shmem_lock(handler);
        {
            if (!shdata->initialized) {
                pthread_barrierattr_t attr;
                assert( pthread_barrierattr_init(&attr) == 0 );
                assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
                assert( pthread_barrier_init(&shdata->barrier, &attr, SYS_SIZE+1) == 0 );
                assert( pthread_barrierattr_destroy(&attr) == 0 );
                shdata->initialized = 1;
            }
        }
        shmem_unlock(handler);

        /* Synchronize: all children are initialized */
        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        /* Parent process checks everything is correct */
        for(i=0; i<SYS_SIZE; ++i) {
            cpu_set_t child_mask, expected_child_mask;
            CPU_ZERO(&expected_child_mask);
            CPU_SET(i, &expected_child_mask);
            assert( DLB_DROM_GetProcessMask(child_pid[i], &child_mask, no_flags)
                    == DLB_SUCCESS );
            assert( CPU_EQUAL(&child_mask, &expected_child_mask) );
        }

        /* Synchronize: all children will finalize */
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

        assert( DLB_DROM_Detach() == DLB_SUCCESS );
    }

    mu_finalize();

    return 0;
}
