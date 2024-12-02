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

#include "assert_loop.h"
#include "unique_shmem.h"

#include "apis/dlb.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* Test --lewi-color */

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

static shmem_handler_t* open_shmem(void **shdata) {
    return shmem_init(shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = SHMEM_KEY,
            });
}


static cpu_set_t process_mask;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &process_mask);
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &process_mask);
}

int main(int argc, char *argv[]) {

    /* Test 4 processes, 2 CPUs each, in a 4 CPU system (oversubscription) */
    {
        enum { NUM_PROCS = 4 };
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, NUM_PROCS) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Create 4 children
        int child_num;
        for (child_num=0; child_num<NUM_PROCS; ++child_num) {
            if (fork() == 0) {
                // Force a 4 CPU system
                enum { SYS_SIZE = 4 };
                mu_init();
                mu_testing_set_sys_size(SYS_SIZE);

                // Initialize DLB
                //  0: [0,1] and 1: [2,3], --lewi-color=1
                //  2: [0,1] and 3: [2,3], --lewi-color=2
                mu_parse_mask(child_num % 2 == 0 ? "0,1" : "2,3", &process_mask);
                char options[64];
                snprintf(options, 64, "--lewi --lewi-color=%1d --shm-key=%s",
                        child_num < 2 ? 1 : 2, SHMEM_KEY);
                assert( DLB_Init(0, &process_mask, options) == DLB_SUCCESS );
                assert( DLB_CallbackSet(dlb_callback_enable_cpu,
                            (dlb_callback_t)cb_enable_cpu, NULL) == DLB_SUCCESS);
                assert( DLB_CallbackSet(dlb_callback_disable_cpu,
                            (dlb_callback_t)cb_disable_cpu, NULL) == DLB_SUCCESS);

                // Barrier
                int error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // Child 0 lends all CPUs: [0,1]
                if (child_num == 0) {
                    assert( DLB_Lend() == DLB_SUCCESS );
                    CPU_ZERO(&process_mask);
                }

                // Barrier
                error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // Child 1 may acquire CPUs 0 and 1
                if (child_num == 1) {
                    assert( DLB_Borrow() == DLB_SUCCESS );
                    assert( CPU_COUNT(&process_mask) == 4 );
                    assert( CPU_ISSET(0, &process_mask)
                            && CPU_ISSET(1, &process_mask) );
                }

                // Barrier
                error = pthread_barrier_wait(&shdata->barrier);
                assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

                // Children 2 and 3 are not affected, they still have their own mask
                if (child_num == 2) {
                    assert( CPU_COUNT(&process_mask) == 2 );
                    assert( CPU_ISSET(0, &process_mask)
                            && CPU_ISSET(1, &process_mask) );
                }
                if (child_num == 3) {
                    assert( CPU_COUNT(&process_mask) == 2 );
                    assert( CPU_ISSET(2, &process_mask)
                            && CPU_ISSET(3, &process_mask) );
                }

                // All 4 processes may invoke the same barrier
                assert( DLB_Barrier() == DLB_SUCCESS );

                // Print shmem from one child of each color:
                if (child_num % 2 == 0) {
                    DLB_PrintShmem(0, DLB_COLOR_ALWAYS);
                }

                assert( DLB_Barrier() == DLB_SUCCESS );

                // Finalize DLB
                assert( DLB_Finalize() == DLB_SUCCESS );
                shmem_finalize(handler, NULL);

                // We need to call _exit so that children don't call assert_shmem destructors,
                // but that prevents gcov reports, so we'll call it if defined
                if (__gcov_flush) __gcov_flush();
                _exit(EXIT_SUCCESS);
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

        // Parent process finalizes barrier shmem
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
    }

    return 0;
}
