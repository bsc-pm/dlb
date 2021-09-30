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

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_policies/lewi.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/debug.h"

#include <string.h>

static int nthreads;

static void cb_set_num_threads(int num_threads, void *arg) {
    nthreads = num_threads;
}

int main( int argc, char **argv ) {
    char options[64] = "--verbose=shmem --shm-key=";
    strcat(options, SHMEM_KEY);

    subprocess_descriptor_t spd;
    spd.id = getpid();
    options_init(&spd.options, options);
    debug_init(&spd.options);
    pm_init(&spd.pm, false /* talp */);

    sched_getaffinity(0, sizeof(cpu_set_t), &spd.process_mask);
    nthreads = CPU_COUNT(&spd.process_mask);
    int original_nthreads = nthreads;

    assert( pm_callback_set(&spd.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)cb_set_num_threads, NULL) == DLB_SUCCESS );

    // Init
    assert( lewi_Init(&spd) == DLB_SUCCESS );
    assert( nthreads == original_nthreads );

    // Lend all CPUs minus 1
    assert( lewi_Lend(&spd) == DLB_SUCCESS );
    assert( nthreads == 1 );

    // Reclaim all
    assert( lewi_Reclaim(&spd) == DLB_SUCCESS );
    assert( nthreads == original_nthreads );

    // Borrow can't obtain CPUs now
    assert( lewi_Borrow(&spd) == DLB_NOUPDT );
    assert( nthreads == original_nthreads );

    // Lend
    assert( lewi_Lend(&spd) == DLB_SUCCESS );
    assert( nthreads == 1 );

    // Borrow some CPUs
    if (original_nthreads >= 3) {
        assert( lewi_BorrowCpus(&spd, 2) == DLB_SUCCESS );
        assert( nthreads == 3 );
    }

    // MaxParallelism test with Borrow
    {
        // Lend
        assert( lewi_Lend(&spd) == DLB_SUCCESS );
        assert( nthreads == 1 );

        // Set MaxParallelism
        assert( lewi_SetMaxParallelism(&spd, 4) == DLB_SUCCESS);

        // Borrow
        if (original_nthreads >= 4) {
            assert( lewi_Borrow(&spd) == DLB_SUCCESS );
            assert( nthreads == 4 );
        }
    }

    // MaxParallelism test with BorrowCpu
    {
        // Lend
        assert( lewi_Lend(&spd) == DLB_SUCCESS );
        assert( nthreads == 1 );

        // Set MaxParallelism
        enum { MAX_PARALLELISM_FACTOR = 4 };
        assert( lewi_SetMaxParallelism(&spd, MAX_PARALLELISM_FACTOR) == DLB_SUCCESS);

        // BorrowCpus one by one
        int i;
        for (i=2; i<=original_nthreads; ++i) {
            int err = lewi_BorrowCpus(&spd, 1);
            if (i<=MAX_PARALLELISM_FACTOR) {
                assert( err == DLB_SUCCESS);
                assert( nthreads == i );
            } else {
                assert( err == DLB_NOUPDT);
                assert( nthreads == MAX_PARALLELISM_FACTOR );
            }
        }
    }

    // Finalize
    assert( lewi_Finalize(&spd) == DLB_SUCCESS );

    return 0;
}
