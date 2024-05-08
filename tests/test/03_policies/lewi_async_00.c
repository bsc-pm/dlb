/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

// LeWI_async checks with 1 processes

#include "assert_loop.h"
#include "unique_shmem.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_async.h"
#include "LB_policies/lewi_async.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <string.h>

static int num_threads;
void set_num_threads_callback(int nthreads, void *arg) {
    num_threads = nthreads;
}

int main(int argc, char *argv[]) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    int initial_num_threads = SYS_SIZE;
    num_threads = SYS_SIZE;

    // Options
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);
    fprintf(stderr, "%s\n", options);

    // Init
    subprocess_descriptor_t spd = {.id = 111, .lewi_ncpus = SYS_SIZE};
    mu_get_system_mask(&spd.process_mask);
    options_init(&spd.options, options);
    debug_init(&spd.options);
    assert( pm_callback_set(&spd.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)set_num_threads_callback, NULL) == DLB_SUCCESS);
    assert( shmem_async_init(spd.id, &spd.pm, &spd.process_mask, spd.options.shm_key)
            == DLB_SUCCESS );
    assert( lewi_async_Init(&spd) == DLB_SUCCESS );

    // Reclaim (this should not trigger a callback)
    assert( lewi_async_Reclaim(&spd) == DLB_NOUPDT );
    assert( num_threads == initial_num_threads );

    // Acquire 1 CPU
    assert( lewi_async_AcquireCpus(&spd, 1) == DLB_NOTED );
    assert( num_threads == initial_num_threads );

    // Remove request
    assert( lewi_async_AcquireCpus(&spd, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // Lend 1 CPU
    assert( lewi_async_LendCpus(&spd, 1) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads - 1 );

    // Lend everything
    assert( lewi_async_Lend(&spd) == DLB_SUCCESS );
    assert( num_threads == 1 );

    // Borrow 1 CPU
    assert( lewi_async_BorrowCpus(&spd, 1) == DLB_SUCCESS );
    assert( num_threads == 2 );

    // Borrow
    assert( lewi_async_Borrow(&spd) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // Lend everything again and Reclaim
    assert( lewi_async_Lend(&spd) == DLB_SUCCESS );
    assert( num_threads == 1 );
    assert( lewi_async_Reclaim(&spd) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // IntoBlockingCall
    assert( lewi_async_IntoBlockingCall(&spd) == DLB_SUCCESS );
    assert( num_threads == 1 );

    // OutOfBlockingCall
    assert( lewi_async_OutOfBlockingCall(&spd) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // Disable
    assert( lewi_async_Disable(&spd) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // Enable
    assert( lewi_async_Enable(&spd) == DLB_SUCCESS );
    assert( num_threads == initial_num_threads );

    // Finalize
    assert( lewi_async_Finalize(&spd) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd.id) == DLB_SUCCESS );

    return 0;
}
