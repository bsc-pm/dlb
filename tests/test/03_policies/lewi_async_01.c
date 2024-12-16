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

// LeWI_async checks with 4 sub-processes

#include "unique_shmem.h"

#include "LB_core/spd.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_lewi_async.h"
#include "LB_policies/lewi_async.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <string.h>

/* Subprocess 1 */
static int sp1_num_threads;
void sp1_set_num_threads_callback(int nthreads, void *arg) {
    sp1_num_threads = nthreads;
}

/* Subprocess 2 */
static int sp2_num_threads;
void sp2_set_num_threads_callback(int nthreads, void *arg) {
    sp2_num_threads = nthreads;
}

/* Subprocess 3 */
static int sp3_num_threads;
void sp3_set_num_threads_callback(int nthreads, void *arg) {
    sp3_num_threads = nthreads;
}

/* Subprocess 4 */
static int sp4_num_threads;
void sp4_set_num_threads_callback(int nthreads, void *arg) {
    sp4_num_threads = nthreads;
}

int main(int argc, char *argv[]) {
    // This test needs at least room for 64 CPUs
    enum { SYS_SIZE = 64 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Options
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);

    // sp1 Init
    int sp1_initial_num_threads = 16;
    sp1_num_threads = sp1_initial_num_threads;
    subprocess_descriptor_t spd1 = {.id = 111, .lewi_ncpus = sp1_initial_num_threads};
    mu_get_system_mask(&spd1.process_mask);
    options_init(&spd1.options, options);
    debug_init(&spd1.options);
    assert( pm_callback_set(&spd1.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)sp1_set_num_threads_callback, NULL) == DLB_SUCCESS );
    assert( shmem_async_init(spd1.id, &spd1.pm, &spd1.process_mask,
                spd1.options.shm_key, spd1.options.shm_size_multiplier) == DLB_SUCCESS );
    assert( lewi_async_Init(&spd1) == DLB_SUCCESS );

    // sp2 Init
    int sp2_initial_num_threads = 16;
    sp2_num_threads = sp2_initial_num_threads;
    subprocess_descriptor_t spd2 = {.id = 222, .lewi_ncpus = sp2_initial_num_threads};
    mu_get_system_mask(&spd2.process_mask);
    assert( pm_callback_set(&spd2.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)sp2_set_num_threads_callback, NULL) == DLB_SUCCESS );
    assert( shmem_async_init(spd2.id, &spd2.pm, &spd2.process_mask,
                spd2.options.shm_key, spd2.options.shm_size_multiplier) == DLB_SUCCESS );
    assert( lewi_async_Init(&spd2) == DLB_SUCCESS );

    // sp3 Init
    int sp3_initial_num_threads = 24;
    sp3_num_threads = sp3_initial_num_threads;
    subprocess_descriptor_t spd3 = {.id = 333, .lewi_ncpus = sp3_initial_num_threads};
    mu_get_system_mask(&spd3.process_mask);
    assert( pm_callback_set(&spd3.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)sp3_set_num_threads_callback, NULL) == DLB_SUCCESS );
    assert( shmem_async_init(spd3.id, &spd3.pm, &spd3.process_mask,
                spd3.options.shm_key, spd3.options.shm_size_multiplier) == DLB_SUCCESS );
    assert( lewi_async_Init(&spd3) == DLB_SUCCESS );

    // sp4 Init
    int sp4_initial_num_threads = 8;
    sp4_num_threads = sp4_initial_num_threads;
    subprocess_descriptor_t spd4 = {.id = 444, .lewi_ncpus = sp4_initial_num_threads};
    mu_get_system_mask(&spd4.process_mask);
    assert( pm_callback_set(&spd4.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)sp4_set_num_threads_callback, NULL) == DLB_SUCCESS );
    assert( shmem_async_init(spd4.id, &spd4.pm, &spd4.process_mask,
                spd4.options.shm_key, spd4.options.shm_size_multiplier) == DLB_SUCCESS );
    assert( lewi_async_Init(&spd4) == DLB_SUCCESS );

    /* Test requests */
    {
        // sp1 wants 5 CPUs
        assert( lewi_async_AcquireCpus(&spd1, 5) == DLB_NOTED );
        assert( sp1_num_threads == sp1_initial_num_threads );

        // every other sp lend 2 CPUs
        assert( lewi_async_LendCpus(&spd2, 2) == DLB_SUCCESS );
        assert( sp2_num_threads == sp2_initial_num_threads - 2 );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads + 2 );

        assert( lewi_async_LendCpus(&spd3, 2) == DLB_SUCCESS );
        assert( sp3_num_threads == sp3_initial_num_threads - 2 );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads + 4 );

        assert( lewi_async_LendCpus(&spd4, 2) == DLB_SUCCESS );
        assert( sp4_num_threads == sp4_initial_num_threads - 2 );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads + 5 );

        // sp2 wants 5 CPUs (gets 1 idle and 1 reclaimed)
        assert( lewi_async_AcquireCpus(&spd2, 5) == DLB_NOTED );
        assert( sp2_num_threads == sp2_initial_num_threads );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads + 4 );

        // everyone resets
        assert( lewi_async_Reclaim(&spd4) == DLB_SUCCESS );
        assert( sp4_num_threads == sp4_initial_num_threads );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads + 2 );
        assert( lewi_async_Reclaim(&spd3) == DLB_SUCCESS );
        assert( sp3_num_threads == sp3_initial_num_threads );
        shmem_async_wait_for_completion(spd1.id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( lewi_async_AcquireCpus(&spd2, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        assert( lewi_async_AcquireCpus(&spd1, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
    }

    /* Test special enums for MAX CPUs and DELETE REQUESTS */
    {
        // Everyone is greedy
        assert( lewi_async_AcquireCpus(&spd1, DLB_MAX_CPUS) == DLB_NOTED );
        assert( lewi_async_AcquireCpus(&spd2, DLB_MAX_CPUS) == DLB_NOTED );
        assert( lewi_async_AcquireCpus(&spd3, DLB_MAX_CPUS) == DLB_NOTED );
        assert( lewi_async_AcquireCpus(&spd4, DLB_MAX_CPUS) == DLB_NOTED );

        // everyone gets into a blocking call
        assert( lewi_async_IntoBlockingCall(&spd1) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads +
                sp1_initial_num_threads / 3 );
        assert( sp3_num_threads == sp3_initial_num_threads +
                sp1_initial_num_threads / 3 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                sp1_initial_num_threads / 3 + 1 ); // +1 not relevant
        assert( lewi_async_IntoBlockingCall(&spd2) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == sp3_initial_num_threads +
                (sp1_initial_num_threads + sp2_initial_num_threads) / 2 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                (sp1_initial_num_threads + sp2_initial_num_threads) / 2 );
        assert( lewi_async_IntoBlockingCall(&spd3) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads ==
                sp1_initial_num_threads + sp2_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( lewi_async_IntoBlockingCall(&spd4) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(spd1.id) == 0 );
        assert( shmem_lewi_async__get_num_requests(spd2.id) == 0 );
        assert( shmem_lewi_async__get_num_requests(spd3.id) == 0 );
        assert( shmem_lewi_async__get_num_requests(spd4.id) == 0 );

        // everyone gets out of the blocking call
        assert( lewi_async_OutOfBlockingCall(&spd1) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads ==
                sp1_initial_num_threads + sp2_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(spd1.id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        assert( lewi_async_OutOfBlockingCall(&spd2) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(spd1.id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        assert( lewi_async_OutOfBlockingCall(&spd3) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads ==
                sp1_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(spd1.id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        assert( lewi_async_OutOfBlockingCall(&spd4) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == sp4_initial_num_threads );
        assert( shmem_lewi_async__get_num_requests(spd1.id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(spd2.id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(spd3.id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(spd4.id) == DLB_MAX_CPUS );

        // sp4 removes requests, others enter a blocking poll
        assert( lewi_async_AcquireCpus(&spd2, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        assert( lewi_async_IntoBlockingCall(&spd1) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads +
                sp1_initial_num_threads / 2 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                sp1_initial_num_threads / 2 );
        assert( shmem_lewi_async__get_num_requests(spd1.id) == 0 );
        assert( shmem_lewi_async__get_num_requests(spd2.id) == 0 );
        assert( shmem_lewi_async__get_num_requests(spd3.id) ==
                DLB_MAX_CPUS - (unsigned)(sp3_num_threads - sp3_initial_num_threads) );
        assert( shmem_lewi_async__get_num_requests(spd4.id) ==
                DLB_MAX_CPUS - (unsigned)(sp4_num_threads - sp4_initial_num_threads) );

        // sp3 removes requests while it has acquired some CPUs, then enters a blocking call
        /* Note: This case is kind of unsupported, or at least it doesn't
         * completely remove the petitions in the queue. The requests are still
         * removed, but if the process has extra resources at this time and
         * lends them, those resources will be added to the queue. Currently we
         * don't have a mechanism to differentiate whether you want to keep or
         * discard the extra resources
         */
        assert( lewi_async_AcquireCpus(&spd3, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        assert( lewi_async_IntoBlockingCall(&spd3) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );

        // sp4 enters a blocking call, and remove requests afterwards
        assert( lewi_async_IntoBlockingCall(&spd4) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( lewi_async_AcquireCpus(&spd4, DLB_DELETE_REQUESTS) == DLB_SUCCESS );

        // sp1, sp3, and sp4 get out of the blocking call
        assert( lewi_async_OutOfBlockingCall(&spd1) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( lewi_async_OutOfBlockingCall(&spd3) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd4.id);
        assert( sp1_num_threads == sp1_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == 1 );
        assert( lewi_async_OutOfBlockingCall(&spd4) == DLB_SUCCESS );
        shmem_async_wait_for_completion(spd1.id);
        shmem_async_wait_for_completion(spd2.id);
        shmem_async_wait_for_completion(spd3.id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == sp4_initial_num_threads );
        assert( shmem_lewi_async__get_num_requests(spd1.id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(spd2.id) == 0 );
        /* Remaining extra CPUs when the DELETE was invoked */
        assert( shmem_lewi_async__get_num_requests(spd3.id)
                == (unsigned)sp1_initial_num_threads / 2 );
        assert( shmem_lewi_async__get_num_requests(spd4.id) == 0 );
    }

    // Finalize
    assert( lewi_async_Finalize(&spd1) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd1.id) == DLB_SUCCESS );
    assert( lewi_async_Finalize(&spd2) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd2.id) == DLB_SUCCESS );
    assert( lewi_async_Finalize(&spd3) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd3.id) == DLB_SUCCESS );
    assert( lewi_async_Finalize(&spd4) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd4.id) == DLB_SUCCESS );

    return 0;
}
