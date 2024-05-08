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

#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_lewi_async.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_sp.h"
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
    char options[64] = "--lewi --mode=async --shm-key=";
    strcat(options, SHMEM_KEY);

    // sp1 Init
    int sp1_initial_num_threads = 16;
    sp1_num_threads = sp1_initial_num_threads;
    dlb_handler_t sp1_handler = DLB_Init_sp(sp1_initial_num_threads, NULL, options);
    assert( sp1_handler != NULL );
    assert( DLB_CallbackSet_sp(sp1_handler, dlb_callback_set_num_threads,
                (dlb_callback_t)sp1_set_num_threads_callback, NULL) == DLB_SUCCESS );
    pid_t sp1_id = ((subprocess_descriptor_t*)sp1_handler)->id;

    // sp2 Init
    int sp2_initial_num_threads = 16;
    sp2_num_threads = sp2_initial_num_threads;
    dlb_handler_t sp2_handler = DLB_Init_sp(sp2_initial_num_threads, NULL, options);
    assert( sp2_handler != NULL );
    assert( DLB_CallbackSet_sp(sp2_handler, dlb_callback_set_num_threads,
                (dlb_callback_t)sp2_set_num_threads_callback, NULL) == DLB_SUCCESS );
    pid_t sp2_id = ((subprocess_descriptor_t*)sp2_handler)->id;

    // sp3 Init
    int sp3_initial_num_threads = 24;
    sp3_num_threads = sp3_initial_num_threads;
    dlb_handler_t sp3_handler = DLB_Init_sp(sp3_initial_num_threads, NULL, options);
    assert( sp3_handler != NULL );
    assert( DLB_CallbackSet_sp(sp3_handler, dlb_callback_set_num_threads,
                (dlb_callback_t)sp3_set_num_threads_callback, NULL) == DLB_SUCCESS );
    pid_t sp3_id = ((subprocess_descriptor_t*)sp3_handler)->id;

    // sp4 Init
    int sp4_initial_num_threads = 8;
    sp4_num_threads = sp4_initial_num_threads;
    dlb_handler_t sp4_handler = DLB_Init_sp(sp4_initial_num_threads, NULL, options);
    assert( sp4_handler != NULL );
    assert( DLB_CallbackSet_sp(sp4_handler, dlb_callback_set_num_threads,
                (dlb_callback_t)sp4_set_num_threads_callback, NULL) == DLB_SUCCESS );
    pid_t sp4_id = ((subprocess_descriptor_t*)sp4_handler)->id;

    sync_call_flags_t flags = (const sync_call_flags_t) {
        .is_mpi = true,
        .is_blocking = true,
        .is_collective = true,
        .do_lewi = true,
    };

    /* Test requests */
    {
        // sp1 wants 5 CPUs
        assert( DLB_AcquireCpus_sp(sp1_handler, 5) == DLB_NOTED );
        assert( sp1_num_threads == sp1_initial_num_threads );

        // every other sp lend 2 CPUs
        assert( DLB_LendCpus_sp(sp2_handler, 2) == DLB_SUCCESS );
        assert( sp2_num_threads == sp2_initial_num_threads - 2 );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads + 2 );

        assert( DLB_LendCpus_sp(sp3_handler, 2) == DLB_SUCCESS );
        assert( sp3_num_threads == sp3_initial_num_threads - 2 );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads + 4 );

        assert( DLB_LendCpus_sp(sp4_handler, 2) == DLB_SUCCESS );
        assert( sp4_num_threads == sp4_initial_num_threads - 2 );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads + 5 );

        // sp2 wants 5 CPUs (gets 1 idle and 1 reclaimed)
        assert( DLB_AcquireCpus_sp(sp2_handler, 5) == DLB_NOTED );
        assert( sp2_num_threads == sp2_initial_num_threads );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads + 4 );

        // everyone resets
        assert( DLB_Reclaim_sp(sp4_handler) == DLB_SUCCESS );
        assert( sp4_num_threads == sp4_initial_num_threads );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads + 2 );
        assert( DLB_Reclaim_sp(sp3_handler) == DLB_SUCCESS );
        assert( sp3_num_threads == sp3_initial_num_threads );
        shmem_async_wait_for_completion(sp1_id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( DLB_AcquireCpus_sp(sp2_handler, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        assert( DLB_AcquireCpus_sp(sp1_handler, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
    }

    /* Test special enums for MAX CPUs and DELETE REQUESTS */
    {
        // Everyone is greedy
        assert( DLB_AcquireCpus_sp(sp1_handler, DLB_MAX_CPUS) == DLB_NOTED );
        assert( DLB_AcquireCpus_sp(sp2_handler, DLB_MAX_CPUS) == DLB_NOTED );
        assert( DLB_AcquireCpus_sp(sp3_handler, DLB_MAX_CPUS) == DLB_NOTED );
        assert( DLB_AcquireCpus_sp(sp4_handler, DLB_MAX_CPUS) == DLB_NOTED );

        // everyone gets into a blocking call
        { spd_enter_dlb(sp1_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads +
                sp1_initial_num_threads / 3 );
        assert( sp3_num_threads == sp3_initial_num_threads +
                sp1_initial_num_threads / 3 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                sp1_initial_num_threads / 3 + 1 ); // +1 not relevant
        { spd_enter_dlb(sp2_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == sp3_initial_num_threads +
                (sp1_initial_num_threads + sp2_initial_num_threads) / 2 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                (sp1_initial_num_threads + sp2_initial_num_threads) / 2 );
        { spd_enter_dlb(sp3_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads ==
                sp1_initial_num_threads + sp2_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        { spd_enter_dlb(sp4_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(sp1_id) == 0 );
        assert( shmem_lewi_async__get_num_requests(sp2_id) == 0 );
        assert( shmem_lewi_async__get_num_requests(sp3_id) == 0 );
        assert( shmem_lewi_async__get_num_requests(sp4_id) == 0 );

        // everyone gets out of the blocking call
        { spd_enter_dlb(sp1_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads ==
                sp1_initial_num_threads + sp2_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == 1 );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(sp1_id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        { spd_enter_dlb(sp2_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(sp1_id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        { spd_enter_dlb(sp3_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads ==
                sp1_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == 1 );
        assert( shmem_lewi_async__get_num_requests(sp1_id)
                == DLB_MAX_CPUS - (unsigned)(sp1_num_threads - sp1_initial_num_threads));
        { spd_enter_dlb(sp4_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == sp4_initial_num_threads );
        assert( shmem_lewi_async__get_num_requests(sp1_id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(sp2_id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(sp3_id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(sp4_id) == DLB_MAX_CPUS );

        // sp4 removes requests, others enter a blocking poll
        assert( DLB_AcquireCpus_sp(sp2_handler, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        { spd_enter_dlb(sp1_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads +
                sp1_initial_num_threads / 2 );
        assert( sp4_num_threads == sp4_initial_num_threads +
                sp1_initial_num_threads / 2 );
        assert( shmem_lewi_async__get_num_requests(sp1_id) == 0 );
        assert( shmem_lewi_async__get_num_requests(sp2_id) == 0 );
        assert( shmem_lewi_async__get_num_requests(sp3_id) ==
                DLB_MAX_CPUS - (unsigned)(sp3_num_threads - sp3_initial_num_threads) );
        assert( shmem_lewi_async__get_num_requests(sp4_id) ==
                DLB_MAX_CPUS - (unsigned)(sp4_num_threads - sp4_initial_num_threads) );

        // sp3 removes requests while it has acquired some CPUs, then enters a blocking call
        /* Note: This case is kind of unsupported, or at least it doesn't
         * completely remove the petitions in the queue. The requests are still
         * removed, but if the process has extra resources at this time and
         * lends them, those resources will be added to the queue. Currently we
         * don't have a mechanism to differentiate whether you want to keep or
         * discard the extra resources
         */
        assert( DLB_AcquireCpus_sp(sp3_handler, DLB_DELETE_REQUESTS) == DLB_SUCCESS );
        { spd_enter_dlb(sp3_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );

        // sp4 enters a blocking call, and remove requests afterwards
        { spd_enter_dlb(sp4_handler); into_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == 1 );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        assert( DLB_AcquireCpus_sp(sp4_handler, DLB_DELETE_REQUESTS) == DLB_SUCCESS );

        // sp1, sp3, and sp4 get out of the blocking call
        { spd_enter_dlb(sp1_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == sp1_initial_num_threads +
                sp3_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == 1 );
        assert( sp4_num_threads == 1 );
        { spd_enter_dlb(sp3_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp4_id);
        assert( sp1_num_threads == sp1_initial_num_threads + sp4_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == 1 );
        { spd_enter_dlb(sp4_handler); out_of_sync_call(flags); }
        shmem_async_wait_for_completion(sp1_id);
        shmem_async_wait_for_completion(sp2_id);
        shmem_async_wait_for_completion(sp3_id);
        assert( sp1_num_threads == sp1_initial_num_threads );
        assert( sp2_num_threads == sp2_initial_num_threads );
        assert( sp3_num_threads == sp3_initial_num_threads );
        assert( sp4_num_threads == sp4_initial_num_threads );
        assert( shmem_lewi_async__get_num_requests(sp1_id) == DLB_MAX_CPUS );
        assert( shmem_lewi_async__get_num_requests(sp2_id) == 0 );
        /* Remaining extra CPUs when the DELETE was invoked */
        assert( shmem_lewi_async__get_num_requests(sp3_id)
                == (unsigned)sp1_initial_num_threads / 2 );
        assert( shmem_lewi_async__get_num_requests(sp4_id) == 0 );
    }

    // Finalize
    assert( DLB_Finalize_sp(sp1_handler) == DLB_SUCCESS );
    assert( DLB_Finalize_sp(sp2_handler) == DLB_SUCCESS );
    assert( DLB_Finalize_sp(sp3_handler) == DLB_SUCCESS );
    assert( DLB_Finalize_sp(sp4_handler) == DLB_SUCCESS );

    return 0;
}
