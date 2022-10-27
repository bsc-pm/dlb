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

    /* Test multiple barriers with one process */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();

        printf("Testing multiple barriers with one process\n");
        int barrier_id_1 = 0;
        int barrier_id_2 = 1;

        shmem_barrier__init(SHMEM_KEY);
        assert( shmem_barrier__attach(barrier_id_1, true) == DLB_SUCCESS );
        assert( shmem_barrier__attach(barrier_id_2, true) == DLB_SUCCESS );

        assert( shmem_barrier__detach(barrier_id_1) == DLB_SUCCESS );
        assert( shmem_barrier__attach(barrier_id_1, true) == DLB_SUCCESS );

        assert( shmem_barrier__attach(barrier_id_2, true) == DLB_NOUPDT );

        assert( shmem_barrier__detach(barrier_id_2) == DLB_SUCCESS );
        assert( shmem_barrier__attach(barrier_id_2, true) == DLB_SUCCESS );

        assert( shmem_barrier__detach(barrier_id_1) == DLB_SUCCESS );
        assert( shmem_barrier__attach(barrier_id_1, true) == DLB_SUCCESS );

        assert( shmem_barrier__detach(barrier_id_2) == DLB_SUCCESS );
        assert( shmem_barrier__detach(barrier_id_2) == DLB_NOUPDT );
        assert( shmem_barrier__attach(barrier_id_2, true) == DLB_SUCCESS );

        assert( shmem_barrier__detach(barrier_id_1) == DLB_SUCCESS );
        assert( shmem_barrier__detach(barrier_id_2) == DLB_SUCCESS );
        shmem_barrier__finalize(SHMEM_KEY);
    }

    /* Test multiple barriers with one process */
    {
        options_t options;
        options_init(&options, NULL);
        debug_init(&options);
        spd_enter_dlb(NULL);
        thread_spd->id = getpid();

        printf("Testing multiple barriers with one process\n");
        int barrier_id_1 = shmem_barrier__get_system_id();
        int barrier_id_2 = barrier_id_1 + 1;

        shmem_barrier__init(SHMEM_KEY);
        assert( shmem_barrier__attach(barrier_id_1, true) == DLB_SUCCESS );
        assert( shmem_barrier__attach(barrier_id_2, true) == DLB_ERR_NOSHMEM );

        assert( shmem_barrier__detach(barrier_id_1) == DLB_SUCCESS );

        shmem_barrier__finalize(SHMEM_KEY);
    }

    return EXIT_SUCCESS;
}
