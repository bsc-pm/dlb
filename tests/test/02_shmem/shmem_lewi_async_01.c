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

// LeWI_async checks with 2+ sub-processes

#include "unique_shmem.h"

#include "LB_comm/shmem_lewi_async.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/mask_utils.h"

#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    pid_t p1_pid = 111;
    unsigned int p1_initial_ncpus = 2;
    pid_t p2_pid = 222;
    unsigned int p2_initial_ncpus = 2;

    unsigned int p1_prev_requested;
    unsigned int p2_prev_requested;
    unsigned int p3_prev_requested;

    unsigned int new_ncpus;
    unsigned int nreqs;
    int max_requests = SYS_SIZE;
    lewi_request_t *requests = malloc(sizeof(lewi_request_t*)*max_requests);

    // Init
    assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
    assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

    /*** Successful ping-pong ***/
    {
        // Process 1 wants 1 CPU
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 lends 1 CPU (P1 gets it)
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 3 );
        assert( p2_prev_requested == 0 );

        // Process 2 reclaims its CPU (P1 is notified to remove one)
        assert( shmem_lewi_async__reclaim(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 2 );

        // Process 2 lends 1 CPU again (P1 still gets it)
        assert( shmem_lewi_async__lend_cpus(p2_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus - 1 );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 3 );
        assert( p2_prev_requested == 0 );

        // Process 2 reclaims its CPU again (P1 sacrifices again)
        assert( shmem_lewi_async__reclaim(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 2 );

        // Process 1 removes petition of 1 CPU
        shmem_lewi_async__remove_requests(p1_pid);

        // Process 2 lends 1 CPU again, P1 petition does not exist anymore
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 2 reclaims its CPU again
        assert( shmem_lewi_async__reclaim(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
    }

    /*** Return acquired CPUs using Lend, request is not renewed  ***/
    {
        // Process 1 wants 1 CPU
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 lends 1 CPU (P1 gets it)
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 3 );
        assert( p2_prev_requested == 0 );

        // Process 1 lends 1 CPU (no one gets it)
        assert( shmem_lewi_async__lend_cpus(p1_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
        assert( p1_prev_requested == 1 );

        // Process 2 lends 1 more CPU, P1 petition does not exist anymore
        assert( shmem_lewi_async__lend_cpus(p2_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus - 2 );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Reset using borrows (because no reason)
        assert( shmem_lewi_async__borrow_cpus(p2_pid, p2_initial_ncpus,
                    &new_ncpus) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
    }

    /*** Reclaim partial CPUs ***/
    {
        // Process 1 lends 2 CPUs
        assert( shmem_lewi_async__lend_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus - 2 );
        assert( nreqs == 0 );
        assert( p1_prev_requested == 0 );

        // Process 2 wants 10 CPUs, only gets 2
        assert( shmem_lewi_async__acquire_cpus(p2_pid, 10, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p2_initial_ncpus + 2 );
        assert( nreqs == 0 );
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 8 );

        // loop 20 times to reclaim 1 and then lend it again to check queue is not reset
        for (int i=0; i<20; ++i) {
            assert( shmem_lewi_async__acquire_cpus(p1_pid, 1, &new_ncpus,
                        requests, &nreqs, max_requests) == DLB_SUCCESS );
            assert( new_ncpus == 1 );
            assert( nreqs == 1 );
            assert( requests[0].pid == p2_pid && requests[0].howmany == 3 );
            assert( shmem_lewi_async__lend_cpus(p1_pid, 1, &new_ncpus, requests,
                        &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
            assert( new_ncpus == 0 );
            assert( nreqs == 1 );
            assert( requests[0].pid == p2_pid && requests[0].howmany == 4 );
            assert( p1_prev_requested == 0 );
        }
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 8 );

        // Process 1 gets its initial CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, p1_initial_ncpus,
                    &new_ncpus, requests, &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p2_pid && requests[0].howmany == p2_initial_ncpus );

        // Process 2 removes requests
        shmem_lewi_async__remove_requests(p2_pid);
    }

    /*** Reset ***/
    {
        // Process 1 resets (no-op)
        assert( shmem_lewi_async__reset(p1_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_NOUPDT );
        assert( p1_prev_requested == 0 );

        // Process 1 wants 2 CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 lends 1 CPU (P1 gets it)
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 3 );
        assert( p2_prev_requested == 0 );

        // Process 1 resets
        assert( shmem_lewi_async__reset(p1_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
        assert( p1_prev_requested == 2 );

        // Process 2 lends 1 more CPU (no one gets it)
        assert( shmem_lewi_async__lend_cpus(p2_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == 0 );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 2 resets
        assert( shmem_lewi_async__reset(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 1 wants 2 CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 lends 2 CPUs again (P1 gets them)
        assert( shmem_lewi_async__lend_cpus(p2_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == 0 );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 4 );
        assert( p2_prev_requested == 0 );

        // Process 2 resets, causing a reclaim
        assert( shmem_lewi_async__reset(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 2 );
        assert( p2_prev_requested == 0 );

        // Process 1 removes petition
        shmem_lewi_async__remove_requests(p1_pid);
    }

    // Finalize
    shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
    assert( new_ncpus == p1_initial_ncpus );
    assert( nreqs == 0 );
    shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
    assert( new_ncpus == p2_initial_ncpus );
    assert( nreqs == 0 );

    /* Re-init and finalize with CPUs lent */
    {
        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 2 lends 1 CPU
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 2 finalizes
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );

        // Process 1 wants 1 CPU (no CPU available)
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 1 finalizes
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
    }

    /* Re-init and finalize with CPUs acquired */
    {
        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 2 lends 1 CPU
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 1 gets 1 CPU
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 1, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus + 1 );
        assert( nreqs == 0 );

        // Process 2 finalizes
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == p2_initial_ncpus );

        // Process 1 finalizes
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
    }

    /* Re-init and finalize with pending requests */
    {
        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 1 wants 2 CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 1 finalizes
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 finalizes
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
    }

    /* Re-init and finalize with acquired external CPUs */
    {
        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 1 wants 2 CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 lends 1 CPU (P1 gets it)
        new_ncpus = p2_initial_ncpus - 1;
        assert( shmem_lewi_async__lend_keep_cpus(p2_pid, new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 1 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 3 );
        assert( p2_prev_requested == 0 );

        // Process 1 finalizes
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );

        // Process 2 finalizes
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
    }

    /* Re-init with more processes, test even distribution when reclaiming */
    {
        enum { NEW_SYS_SIZE = 64 };
        mu_testing_set_sys_size(NEW_SYS_SIZE);

        p1_initial_ncpus = 16;
        p2_initial_ncpus = 16;
        pid_t p3_pid = 333;
        unsigned int p3_initial_ncpus = 16;
        pid_t p4_pid = 444;
        unsigned int p4_initial_ncpus = 16;

        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p3_pid, p3_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p4_pid, p4_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 3 lends all
        assert( shmem_lewi_async__lend_keep_cpus(p3_pid, 0, requests,
                    &nreqs, max_requests, &p3_prev_requested) == DLB_SUCCESS );
        assert( nreqs == 0 );
        assert( p3_prev_requested == 0 );

        // Process 1 gets 3 CPUs
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 3, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus + 3 );
        assert( nreqs == 0 );
        assert( shmem_lewi_async__get_num_requests(p1_pid) == 0 );

        // Process 2 gets 8 CPUs
        assert( shmem_lewi_async__acquire_cpus(p2_pid, 8, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus + 8 );
        assert( nreqs == 0 );
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 0 );

        // Process 4 wants 20 CPUs, only gets 5
        assert( shmem_lewi_async__acquire_cpus(p4_pid, 20, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p4_initial_ncpus + 5 );
        assert( nreqs == 0 );
        assert( shmem_lewi_async__get_num_requests(p4_pid) == 15 );

        /* Status:
         *  P1: 16 + 3
         *  P2: 16 + 8
         *  P3: 0
         *  P4: 16 + 5 (+15 requested)
         */

        // Process 3 now reclaims 4 CPUs, which should be reclaimed evenly */
        assert( shmem_lewi_async__acquire_cpus(p3_pid, 4, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == 4 );
        assert( nreqs == 3 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 18 );
        assert( requests[1].pid == p4_pid && requests[1].howmany == 20 );
        assert( requests[2].pid == p2_pid && requests[2].howmany == 22 );
        assert( shmem_lewi_async__get_num_requests(p1_pid) == 1 );
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 2 );
        assert( shmem_lewi_async__get_num_requests(p3_pid) == 0 );
        assert( shmem_lewi_async__get_num_requests(p4_pid) == 16 );

        /* Status:
         *  P1: 16 + 2 (+1 requested)
         *  P2: 16 + 6 (+2 requested)
         *  P3: 4
         *  P4: 16 + 4 (+16 requested)
         */

        // Process 3 now reclaims 10 CPUs, which should be reclaimed evenly */
        assert( shmem_lewi_async__acquire_cpus(p3_pid, 10, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == 14 );
        assert( nreqs == 3 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 16 );
        assert( requests[1].pid == p4_pid && requests[1].howmany == 16 );
        assert( requests[2].pid == p2_pid && requests[2].howmany == 18 );
        assert( shmem_lewi_async__get_num_requests(p1_pid) == 3 );
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 6 );
        assert( shmem_lewi_async__get_num_requests(p3_pid) == 0 );
        assert( shmem_lewi_async__get_num_requests(p4_pid) == 20 );

        /* Status:
         *  P1: 16     (+3 requested)
         *  P2: 16 + 2 (+6 requested)
         *  P3: 14
         *  P4: 16     (+20 requested)
         */

        // Process 2 resets
        assert( shmem_lewi_async__reset(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 2 );
        assert( requests[0].pid == p1_pid && requests[0].howmany == 17 );
        assert( requests[1].pid == p4_pid && requests[1].howmany == 17 );
        assert( p2_prev_requested == 8 );

        // Process 3 reclaims last 2 CPUs
        assert( shmem_lewi_async__acquire_cpus(p3_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( nreqs == 2 );
        assert( requests[0].pid == p4_pid && requests[0].howmany == 16 );
        assert( requests[1].pid == p1_pid && requests[1].howmany == 16 );

        // Remove petitions
        assert( shmem_lewi_async__get_num_requests(p1_pid) == 3 );
        assert( shmem_lewi_async__get_num_requests(p2_pid) == 0 );
        assert( shmem_lewi_async__get_num_requests(p3_pid) == 0 );
        assert( shmem_lewi_async__get_num_requests(p4_pid) == 20 );
        shmem_lewi_async__remove_requests(p1_pid);
        shmem_lewi_async__remove_requests(p4_pid);

        // Finalize
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
        shmem_lewi_async__finalize(p3_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p3_initial_ncpus );
        assert( nreqs == 0 );
        shmem_lewi_async__finalize(p4_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p4_initial_ncpus );
        assert( nreqs == 0 );
    }

    /* Re-init with more processes, test reclaim on mixed (idle and borrowed) CPUs */
    {
        enum { NEW_SYS_SIZE = 64 };
        mu_testing_set_sys_size(NEW_SYS_SIZE);

        p1_initial_ncpus = 16;
        p2_initial_ncpus = 16;
        pid_t p3_pid = 333;
        unsigned int p3_initial_ncpus = 16;

        // Init
        assert( shmem_lewi_async__init(p1_pid, p1_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p2_pid, p2_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );
        assert( shmem_lewi_async__init(p3_pid, p3_initial_ncpus, SHMEM_KEY,
                    SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        // Process 1 and 2 lend 2 CPUs each
        assert( shmem_lewi_async__lend_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus - 2 );
        assert( nreqs == 0 );
        assert( p1_prev_requested == 0 );
        assert( shmem_lewi_async__lend_cpus(p2_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus - 2 );
        assert( nreqs == 0 );
        assert( p2_prev_requested == 0 );

        // Process 3 gets 3 CPUs (1 still idle)
        assert( shmem_lewi_async__acquire_cpus(p3_pid, 3, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_SUCCESS );
        assert( new_ncpus == p3_initial_ncpus + 3 );
        assert( nreqs == 0 );

        // Test 1: with Acquire
        // Process 1 wants 3 CPUs (1 is idle, 1 is reclaimed, 1 is requested)
        assert( shmem_lewi_async__acquire_cpus(p1_pid, 3, &new_ncpus, requests,
                    &nreqs, max_requests) == DLB_NOTED );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus + 2 );

        // Process 1 removes request and lends again 2 CPUs (P3 gets one)
        shmem_lewi_async__remove_requests(p1_pid);
        assert( shmem_lewi_async__lend_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus - 2 );
        assert( nreqs == 1 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus + 3 );
        assert( p1_prev_requested == 0 );

        // Test 2: with Reclaim
        // Process 1 reclaims (1 is idle, 1 is reclaimed)
        assert( shmem_lewi_async__reclaim(p1_pid, &new_ncpus, requests,
                    &nreqs, max_requests, p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus + 2 );

        // Process 1 removes request and lends again 2 CPUs (P3 gets one)
        shmem_lewi_async__remove_requests(p1_pid);
        assert( shmem_lewi_async__lend_cpus(p1_pid, 2, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus - 2 );
        assert( nreqs == 1 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus + 3 );
        assert( p1_prev_requested == 0 );

        // Test 3: with Reset
        // Process 1 resets (1 is idle, 1 is reclaimed)
        assert( shmem_lewi_async__reset(p1_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p1_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 1 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus + 2 );
        assert( p1_prev_requested == 0 );

        // Process 2 and 3 reset
        assert( shmem_lewi_async__reset(p2_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p2_prev_requested) == DLB_SUCCESS );
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 1 );
        assert( p2_prev_requested == 0 );
        assert( requests[0].pid == p3_pid && requests[0].howmany == p3_initial_ncpus );
        assert( shmem_lewi_async__reset(p3_pid, &new_ncpus, requests,
                    &nreqs, max_requests, &p3_prev_requested) == DLB_NOUPDT );
        assert( new_ncpus == p3_initial_ncpus );
        assert( nreqs == 0 );
        assert( p3_prev_requested == 0 );

        // Finalize
        shmem_lewi_async__finalize(p1_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p1_initial_ncpus );
        assert( nreqs == 0 );
        shmem_lewi_async__finalize(p2_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p2_initial_ncpus );
        assert( nreqs == 0 );
        shmem_lewi_async__finalize(p3_pid, &new_ncpus, requests, &nreqs, max_requests);
        assert( new_ncpus == p3_initial_ncpus );
        assert( nreqs == 0 );
    }

    return 0;
}
