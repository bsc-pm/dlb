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
#include "apis/dlb_types.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>


/* Test node barrier */

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

int main(int argc, char **argv) {

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    /* Test multiple barriers with one process */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();

        printf("Testing multiple barriers with one process\n");

        shmem_barrier__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
        int max_barriers = shmem_barrier__get_max_barriers();
        barrier_t *barrier0 = shmem_barrier__register("Barrier 0", DLB_BARRIER_LEWI_OFF);
        assert( barrier0 != NULL );
        if (max_barriers > 2) {
            barrier_t *barrier1 = shmem_barrier__register("Barrier 1", DLB_BARRIER_LEWI_OFF);
            assert( barrier1 != NULL );
            barrier_t *barrier2 = shmem_barrier__register("Barrier 2", DLB_BARRIER_LEWI_ON);
            assert( barrier2 != NULL );
            barrier_t *barrier3 = shmem_barrier__register("Barrier 2", DLB_BARRIER_LEWI_ON);
            assert( barrier2 == barrier3 );
            assert( shmem_barrier__detach(barrier3) == 1 );

            assert( shmem_barrier__attach(barrier2) == 2 );
            assert( shmem_barrier__attach(barrier2) == 3 );
            assert( shmem_barrier__detach(barrier2) == 2 );
            assert( shmem_barrier__detach(barrier2) == 1 );

            assert( shmem_barrier__detach(barrier1) == 0 );
            assert( shmem_barrier__detach(barrier2) == 0 );
        }

        assert( shmem_barrier__detach(barrier0) == 0 );
        shmem_barrier__finalize(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
    }

    /* Test multiple barriers with one process */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();
        shmem_barrier__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);

        printf("Testing multiple barriers with one process\n");
        uint16_t max_barriers = (uint16_t)shmem_barrier__get_max_barriers();
        barrier_t **barrier_list = calloc(max_barriers+1, sizeof(barrier_t*));
        char barrier_name[16];
        bool lewi = false;

        /* Register MAX+1 barriers (twice to check the detach actually frees up space) */
        int i, j;
        for (j=0; j<2; ++j) {
            for (i=0; i<max_barriers; ++i) {
                snprintf(barrier_name, 16, "Barrier %6d", i);
                barrier_list[i] = shmem_barrier__register(barrier_name, lewi);
                assert( barrier_list[i] != NULL );
            }
            snprintf(barrier_name, 16, "Doesn't fit");
            barrier_list[i] = shmem_barrier__register(barrier_name, lewi);
            assert( barrier_list[i] == NULL );
            for (i=0; i<max_barriers; ++i) {
                assert( shmem_barrier__detach(barrier_list[i]) == 0 );
            }
        }

        shmem_barrier__finalize(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER);
    }

    return EXIT_SUCCESS;
}
