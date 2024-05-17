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

// Basic checks with 1 sub-process

#include "unique_shmem.h"

#include "LB_comm/shmem_lewi_async.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "support/mask_utils.h"

#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

    pid_t pid = 111;
    int system_size = mu_get_system_size();
    unsigned int new_ncpus;
    unsigned int nreqs;
    unsigned int prev_requested;
    unsigned int initial_ncpus = system_size;
    int max_requests = system_size;
    lewi_request_t *requests = malloc(sizeof(lewi_request_t*)*max_requests);

    // no shmem yet
    assert( shmem_lewi_async__lend_cpus(pid, 1, NULL, NULL, NULL, 0, NULL)
            == DLB_ERR_NOSHMEM );
    assert( shmem_lewi_async__lend_keep_cpus(pid, 1, NULL, NULL, 0, NULL)
            == DLB_ERR_NOSHMEM );
    assert( shmem_lewi_async__reclaim(pid, NULL, NULL, NULL, 0, 0)
            == DLB_ERR_NOSHMEM );
    assert( shmem_lewi_async__acquire_cpus(pid, system_size, NULL, NULL, NULL, 0)
            == DLB_ERR_NOSHMEM );
    assert( shmem_lewi_async__borrow_cpus(pid, system_size, NULL)
            == DLB_ERR_NOSHMEM );
    shmem_lewi_async__remove_requests(pid);

    shmem_lewi_async__init(pid, initial_ncpus, SHMEM_KEY);
    assert( shmem_lewi_async__exists() );

    /* Lend cpus */
    assert( shmem_lewi_async__lend_cpus(pid, 0, &new_ncpus, requests, &nreqs,
                max_requests, &prev_requested) == DLB_NOUPDT );
    assert( nreqs == 0 );
    assert( new_ncpus == 0 );
    assert( prev_requested == 0 );
    assert( shmem_lewi_async__lend_cpus(pid, 1, &new_ncpus, requests, &nreqs,
                max_requests, &prev_requested) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( new_ncpus == initial_ncpus - 1 );
    assert( prev_requested == 0 );
    assert( shmem_lewi_async__lend_cpus(pid, system_size, &new_ncpus, requests,
                &nreqs, max_requests, &prev_requested) == DLB_ERR_PERM );
    assert( shmem_lewi_async__lend_cpus(pid+1, 1, &new_ncpus, requests, &nreqs,
                max_requests, &prev_requested) == DLB_ERR_NOPROC );
    assert( shmem_lewi_async__reclaim(pid, &new_ncpus, requests, &nreqs,
                max_requests, prev_requested) == DLB_SUCCESS );
    assert( new_ncpus == initial_ncpus );

    /* Lend up to ncpus */
    assert( shmem_lewi_async__lend_keep_cpus(pid, 1, requests, &nreqs,
                max_requests, &prev_requested) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( prev_requested == 0 );
    assert( shmem_lewi_async__lend_keep_cpus(pid, 1, requests, &nreqs,
                max_requests, &prev_requested) == DLB_NOUPDT );
    assert( nreqs == 0 );
    assert( prev_requested == 0 );
    assert( shmem_lewi_async__lend_keep_cpus(pid, system_size, requests, &nreqs,
                max_requests, &prev_requested) == DLB_ERR_PERM );
    assert( shmem_lewi_async__lend_keep_cpus(pid+1, 1, requests, &nreqs,
                max_requests, &prev_requested) == DLB_ERR_NOPROC );
    assert( shmem_lewi_async__reclaim(pid, &new_ncpus, requests, &nreqs,
                max_requests, prev_requested) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( new_ncpus == initial_ncpus );

    /* Reclaim */
    assert( shmem_lewi_async__reclaim(pid, &new_ncpus, requests, &nreqs,
                max_requests, prev_requested) == DLB_NOUPDT );
    assert( new_ncpus == initial_ncpus );
    assert( shmem_lewi_async__reclaim(pid+1, NULL, NULL, NULL, 0, 0) == DLB_ERR_NOPROC );

    /* Acquire */
    assert( shmem_lewi_async__acquire_cpus(pid, 0, NULL, NULL, NULL, 0) == DLB_NOUPDT );
    assert( shmem_lewi_async__acquire_cpus(pid+1, 1, NULL, NULL, NULL, 0) == DLB_ERR_NOPROC );
    assert( shmem_lewi_async__acquire_cpus(pid, 1, &new_ncpus, requests, &nreqs,
                max_requests) == DLB_NOTED );
    assert( nreqs == 0 );
    assert( shmem_lewi_async__acquire_cpus(pid, 2, &new_ncpus, requests, &nreqs,
                max_requests) == DLB_NOTED );
    assert( new_ncpus == initial_ncpus );
    shmem_lewi_async__remove_requests(pid);

    /* Borrow */
    assert( shmem_lewi_async__borrow_cpus(pid, 0, &new_ncpus) == DLB_NOUPDT );
    assert( new_ncpus == initial_ncpus );
    assert( shmem_lewi_async__borrow_cpus(pid+1, 1, NULL) == DLB_ERR_NOPROC );
    assert( shmem_lewi_async__borrow_cpus(pid, 0, &new_ncpus) == DLB_NOUPDT );
    assert( new_ncpus == initial_ncpus );

    /* Lend + Acquire */
    assert( shmem_lewi_async__lend_keep_cpus(pid, 0, requests, &nreqs,
                max_requests, &prev_requested) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( prev_requested == 0 );
    assert( shmem_lewi_async__acquire_cpus(pid, 1, &new_ncpus, requests, &nreqs,
                max_requests) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( new_ncpus == 1 );
    if (initial_ncpus > 1) {
        assert( shmem_lewi_async__acquire_cpus(pid, initial_ncpus-1, &new_ncpus,
                    requests, &nreqs, max_requests) == DLB_SUCCESS );
        assert( nreqs == 0 );
        assert( new_ncpus == initial_ncpus );
    }

    /* Acquire (add requests) + Lend */
    assert( shmem_lewi_async__acquire_cpus(pid, DLB_MAX_CPUS, &new_ncpus,
                requests, &nreqs, max_requests) == DLB_NOTED );
    assert( nreqs == 0 );
    assert( new_ncpus == initial_ncpus );
    assert( shmem_lewi_async__get_num_requests(pid) == DLB_MAX_CPUS );
    assert( shmem_lewi_async__lend_keep_cpus(pid, 0, requests, &nreqs,
                max_requests, &prev_requested) == DLB_SUCCESS );
    assert( nreqs == 0 );
    assert( prev_requested == DLB_MAX_CPUS );
    assert( shmem_lewi_async__reclaim(pid, &new_ncpus, requests, &nreqs,
                max_requests, prev_requested) == DLB_SUCCESS );
    assert( new_ncpus == initial_ncpus );
    assert( shmem_lewi_async__get_num_requests(pid) == DLB_MAX_CPUS );


    shmem_lewi_async__finalize(pid, &new_ncpus, requests, &nreqs, max_requests);
    assert( new_ncpus == initial_ncpus );
    assert( nreqs == 0 );
    assert( !shmem_lewi_async__exists() );

    return 0;
}
