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
    assert( shmem_procinfo__init(p1_pid, 0, &p1_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );

    // Initialize sub-process 2
    pid_t p2_pid = 222;
    cpu_set_t p2_mask;
    memcpy(&p2_mask, &original_p2_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(p2_pid, 0, &p2_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );

    // Initialize external
    assert( shmem_procinfo_ext__init(SHMEM_KEY) == DLB_SUCCESS );

    // Sub-process 3
    pid_t p3_pid = 333;
    cpu_set_t p3_mask = { .__bits = {0x6}}; /* [0110] */

    // no stealing
    {
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask, 0) == DLB_ERR_PERM );
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

    // stealing with ERROR
    {
        /* CPU 0 could be stolen, but 2 & 3 not since p2_mask cannot be left empty  */
        cpu_set_t p3_ilegal_mask = { .__bits = {0xd} }; /* [1101] */

        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_ilegal_mask, DLB_STEAL_CPUS)
                == DLB_ERR_PERM );
        assert( shmem_procinfo__polldrom(p1_pid, NULL, NULL) == DLB_NOUPDT );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, NULL) == DLB_NOUPDT );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // synchronous
    {
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask, DLB_STEAL_CPUS | DLB_SYNC_QUERY)
                == DLB_ERR_TIMEOUT );
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
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_mask, DLB_STEAL_CPUS | DLB_SYNC_QUERY)
                == DLB_SUCCESS );
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

    // synchronous while p1 and p2 are polling (ERROR)
    {
        pthread_barrier_init(&barrier, NULL, 3);

        struct thread_data td1 = { .pid = p1_pid, .mask = &p1_mask, .terminate = false };
        pthread_t thread1;
        pthread_create(&thread1, NULL, poll_drom, &td1);

        struct thread_data td2 = { .pid = p2_pid, .mask = &p2_mask, .terminate = false };
        pthread_t thread2;
        pthread_create(&thread2, NULL, poll_drom, &td2);

        /* CPU 0 could be stolen, but 2 & 3 not since p2_mask cannot be left empty  */
        cpu_set_t p3_ilegal_mask = { .__bits = {0xd} }; /* [1101] */

        pthread_barrier_wait(&barrier);

        // preinitialize and check masks
        assert( shmem_procinfo_ext__preinit(p3_pid, &p3_ilegal_mask,
                    DLB_STEAL_CPUS | DLB_SYNC_QUERY) == DLB_ERR_PERM );

        // P2 mask surely didn't get updated, thread 2 should still be polling
        assert( pthread_tryjoin_np(thread2, NULL) != 0 );
        td2.terminate = true;
        pthread_join(thread2, NULL);

        // P1 mask is either not updated (rollback was fast enough and cleared dirty flag)
        // or it was updated and we need to update again
        td1.terminate = true;
        pthread_join(thread1, NULL);
        cpu_set_t mask;
        int err = shmem_procinfo__polldrom(p1_pid, NULL, &mask);
        if (err == DLB_NOUPDT) {
            /* p1 did not succesfully poll, nothing to revert */
        } else if (err == DLB_SUCCESS) {
            /* p1 did poll and mask need to be updated again */
            memcpy(&p1_mask, &mask, sizeof(cpu_set_t));
        } else {
            /* unkown */
            return EXIT_FAILURE;
        }

        pthread_barrier_destroy(&barrier);

        // check p1 and p2 masks are correct
        assert( shmem_procinfo__polldrom(p1_pid, NULL, NULL) == DLB_NOUPDT );
        assert( CPU_EQUAL(&p1_mask, &original_p1_mask) );
        assert( shmem_procinfo__polldrom(p2_pid, NULL, NULL) == DLB_NOUPDT );
        assert( CPU_EQUAL(&p2_mask, &original_p2_mask) );
    }

    // Finalize sub-processes
    assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY) == DLB_SUCCESS );

    // Finalize external
    assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );

    return 0;
}
