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
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

// PreInit tests

struct thread_data {
    pid_t pid;
    cpu_set_t *mask;
    bool terminate;
};

static pthread_barrier_t barrier;

static void* poll_drom(void *arg) {
    pid_t pid = ((struct thread_data*)arg)->pid;
    cpu_set_t *mask = ((struct thread_data*)arg)->mask;
    bool *terminate = &((struct thread_data*)arg)->terminate;

    pthread_barrier_wait(&barrier);

    int err;
    cpu_set_t local_mask;
    CPU_ZERO(&local_mask);

    while ((err = shmem_procinfo__polldrom(pid, NULL, &local_mask)) != DLB_SUCCESS
            && !*terminate) {
        usleep(1000);
    }

    if (err == DLB_SUCCESS) {
        memcpy(mask, &local_mask, sizeof(cpu_set_t));
    }

    return NULL;
}

int main( int argc, char **argv ) {

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    const cpu_set_t original_p1_mask = { .__bits = {0x3} }; /* [0011] */
    const cpu_set_t original_p2_mask = { .__bits = {0xc} }; /* [1100] */

    // Initialize sub-process 1
    pid_t p1_pid = 111;
    cpu_set_t p1_mask;
    memcpy(&p1_mask, &original_p1_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(p1_pid, 0, &p1_mask, NULL, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

    // Initialize sub-process 2
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    memcpy(&p2_mask, &original_p2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

    // Initialize external
    assert( shmem_procinfo_ext__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

    // Sub-process 3
    pid_t p3_pid = 333;
    cpu_set_t p3_mask = { .__bits = {0x6}}; /* [0110] */

    dlb_drom_flags_t no_flags = DLB_DROM_FLAGS_NONE;

    // no stealing
    {
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask, no_flags) == DLB_ERR_PERM );
    }

    // stealing
    {
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask, DLB_STEAL_CPUS) == DLB_SUCCESS );
        assert( shmem_procinfo_ext__postfinalize(p3_pid, /* return-stolen */ true) == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, &p2_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // stealing all CPUs (this is no longer ERROR)
    {
        cpu_set_t p3_new_mask = { .__bits = {0xf} }; /* [1111] */

        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_new_mask, DLB_STEAL_CPUS)
                == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_COUNT(&p1_mask) == 0 );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, &p2_mask) == DLB_SUCCESS );
        assert( CPU_COUNT(&p2_mask) == 0 );

        // roll back
        assert( shmem_procinfo_ext__postfinalize(p3_pid, /* return_stolen */ true) == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, &p2_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // stealing with dirty process (ERROR)
    {
        cpu_set_t p3_new_mask;
        mu_parse_mask("0", &p3_new_mask);
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_new_mask, DLB_STEAL_CPUS)
                == DLB_SUCCESS );
        mu_parse_mask("0-1", &p3_new_mask);
        /* error because p1 is still dirty */
        assert( shmem_procinfo__setprocessmask(p3_pid, &p3_new_mask, DLB_STEAL_CPUS, NULL)
                == DLB_ERR_PERM );

        // roll back
        assert( shmem_procinfo_ext__postfinalize(p3_pid, /* return_stolen */ true) == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
    }

    // synchronous
    {
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask,
                    (dlb_drom_flags_t)(DLB_STEAL_CPUS | DLB_SYNC_QUERY)) == DLB_ERR_TIMEOUT );
    }

    // synchronous while p1 and p2 are polling
    {
        pthread_barrier_init(&barrier, NULL, 3);

        struct thread_data td1 = { .pid = p1_pid, .mask = &p1_mask, .terminate = false };
        pthread_t thread1;
        pthread_create(&thread1, NULL, poll_drom, &td1);

        struct thread_data td2 = { .pid = p2_pid, .mask = &p2_mask, .terminate = false };
        pthread_t thread2;
        pthread_create(&thread2, NULL, poll_drom, &td2);

        pthread_barrier_wait(&barrier);

        // preinitialize and check masks
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask,
                    (dlb_drom_flags_t)(DLB_STEAL_CPUS | DLB_SYNC_QUERY)) == DLB_SUCCESS );
        assert( CPU_COUNT(&p1_mask) == 1 && CPU_ISSET(0, &p1_mask) );
        assert( CPU_COUNT(&p2_mask) == 1 && CPU_ISSET(3, &p2_mask) );
        assert( CPU_COUNT(&p3_mask) == 2 && CPU_ISSET(1, &p3_mask) && CPU_ISSET(2, &p3_mask) );

        if (pthread_tryjoin_np(thread1, NULL) != 0
                || pthread_tryjoin_np(thread2, NULL) != 0) {
            return EXIT_FAILURE;
        }
        pthread_barrier_destroy(&barrier);

        // postfinalize and recover
        assert( shmem_procinfo_ext__postfinalize(p3_pid, /* return-stolen */ true) == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, &p2_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // synchronous while p1 and p2 are polling. All CPUs, no longer ERROR
    {
        pthread_barrier_init(&barrier, NULL, 3);

        struct thread_data td1 = { .pid = p1_pid, .mask = &p1_mask, .terminate = false };
        pthread_t thread1;
        pthread_create(&thread1, NULL, poll_drom, &td1);

        struct thread_data td2 = { .pid = p2_pid, .mask = &p2_mask, .terminate = false };
        pthread_t thread2;
        pthread_create(&thread2, NULL, poll_drom, &td2);

        cpu_set_t p3_new_mask = { .__bits = {0xf} }; /* [1111] */

        pthread_barrier_wait(&barrier);

        // preinitialize and check masks
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_new_mask,
                    (dlb_drom_flags_t)(DLB_STEAL_CPUS | DLB_SYNC_QUERY)) == DLB_SUCCESS );
        assert( CPU_COUNT(&p1_mask) == 0 );
        assert( CPU_COUNT(&p2_mask) == 0 );
        cpu_set_t mask;
        assert( shmem_procinfo__getprocessmask(p3_pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &p3_new_mask) );

        if (pthread_tryjoin_np(thread1, NULL) != 0
                || pthread_tryjoin_np(thread2, NULL) != 0) {
            return EXIT_FAILURE;
        }
        pthread_barrier_destroy(&barrier);

        // postfinalize and recover
        assert( shmem_procinfo_ext__postfinalize(p3_pid, /* return-stolen */ true) == DLB_SUCCESS );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &p1_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, &p2_mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // Finalize sub-processes
    assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
            == DLB_SUCCESS );

    // Partial CPU inheritance: [0-3] -> [1-3],[0] -> [2-3],[0],[1]
    {
        cpu_set_t mask;
        cpu_set_t expected_mask;

        // parent process (p1) pre-initializes full mask
        mu_parse_mask("0-3", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p2 inherits CPU 0
        mu_parse_mask("0", &p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p2_mask, &mask) );

        // p1 still owns CPUs [1-3]
        mu_parse_mask("1-3", &expected_mask);
        assert( shmem_procinfo__getprocessmask(p1_pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &expected_mask) );

        // p3 inherits CPU 1
        mu_parse_mask("1", &p3_mask);
        assert( shmem_procinfo__init(p3_pid, p1_pid, &p3_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p3_mask, &mask) );

        // p1 still owns CPUs [2-3]
        mu_parse_mask("2-3", &expected_mask);
        assert( shmem_procinfo__getprocessmask(p1_pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &expected_mask) );

        // CPUs [2-3] are still not inherited, clean up is necessary
        assert( shmem_procinfo_ext__postfinalize(p1_pid, false) == DLB_SUCCESS );

        // finalize
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Inheritance + expansion: [0-2] -> [0-3]
    {
        cpu_set_t mask;

        // parent process (p1) pre-initializes [0-2]
        mu_parse_mask("0-2", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p2 wants to register [0-3]
        mu_parse_mask("0-3", &p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p2_mask, &mask) );

        // p1 is not registered anymore
        assert( shmem_procinfo__getprocessmask(p1_pid, &mask, no_flags) == DLB_ERR_NOPROC );

        // finalize
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Inheritance + expansion: [0-2],[3] -> [0-3] (ERROR)
    {
        cpu_set_t mask;

        // parent process (p1) pre-initializes [0-2]
        mu_parse_mask("0-2", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p3 registers CPU [3]
        mu_parse_mask("3", &p3_mask);
        assert( shmem_procinfo__init(p3_pid, 0, &p3_mask, NULL, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // p2 wants to register [0-3], ERROR
        mu_parse_mask("0-3", &p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_ERR_PERM );

        // CPUs [0-2] have not been successfully inherited, clean up
        assert( shmem_procinfo_ext__postfinalize(p1_pid, false) == DLB_SUCCESS );

        // finalize
        assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Partial inheritance + expansion: [0,2] -> [2],[0-1] -> [0-1],[2-3]
    {
        cpu_set_t mask;

        // parent process (p1) pre-initializes [0,2]
        mu_parse_mask("0,2", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p2 wants to register [0-1]
        mu_parse_mask("0-1", &p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p2_mask, &mask) );

        // p3 wants to register [2-3]
        mu_parse_mask("2-3", &p3_mask);
        assert( shmem_procinfo__init(p3_pid, p1_pid, &p3_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p3_mask, &mask) );

        // p1 is not registered anymore
        assert( shmem_procinfo__getprocessmask(p1_pid, &mask, no_flags) == DLB_ERR_NOPROC );

        // finalize
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Inheritance beyond the initial mask: [0] -> [0] -> [0], [1]
    {
        cpu_set_t mask;

        // parent process (p1) pre-initializes [0]
        mu_parse_mask("0", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p2 wants to register [0]
        mu_parse_mask("0", &p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p2_mask, &mask) );

        // p3 wants to register [1]  (with p1_pid as preinit!)
        mu_parse_mask("1", &p3_mask);
        assert( shmem_procinfo__init(p3_pid, p1_pid, &p3_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        /* mask is only updated if DLB_NOTED */

        // check that only p2 and p3 are registered
        assert( shmem_procinfo__getprocessmask(p1_pid, &mask, no_flags) == DLB_ERR_NOPROC );
        assert( shmem_procinfo__getprocessmask(p2_pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p2_mask, &mask) );
        assert( shmem_procinfo__getprocessmask(p3_pid, &mask, no_flags) == DLB_SUCCESS );
        assert( CPU_EQUAL(&p3_mask, &mask) );

        // finalize
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(p3_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Inheritance with an empty mask
    {
        cpu_set_t mask;

        // parent process (p1) pre-initializes [0]
        mu_parse_mask("0", &p1_mask);
        assert( shmem_procinfo_ext__preinit(p1_pid, &p1_mask, no_flags) == DLB_SUCCESS );

        // p2 wants to register with an empty mask
        CPU_ZERO(&p2_mask);
        assert( shmem_procinfo__init(p2_pid, p1_pid, &p2_mask, &mask, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_NOTED );
        assert( CPU_EQUAL(&p1_mask, &mask) );

        // finalize
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY, SHMEM_SIZE_MULTIPLIER)
                == DLB_SUCCESS );
    }

    // Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    return 0;
}
