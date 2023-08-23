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

#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <pthread.h>
#include <sched.h>
#include <string.h>

/* Test generic LeWI functions through DLB_kernel */

/* Callbacks to count how many times they have been called */
static int ntimes = 0;
static void cb_enable_cpu(int cpuid, void *arg) {
    ++ntimes;
}
static void cb_disable_cpu(int cpuid, void *arg) {
    ++ntimes;
}
static void cb_set_num_threads(int num_threads, void *arg) {
    ++ntimes;
}

static void* observer_func(void *arg) {
    /* Set up observer flag */
    set_observer_role(true);

    int prev_ntimes = ntimes;
    sync_call_flags_t flags = (const sync_call_flags_t) {
        .is_mpi = true,
        .is_blocking = true,
        .is_collective = true,
        .do_lewi = true,
    };
    into_sync_call(flags);
    out_of_sync_call(flags);
    assert( prev_ntimes == ntimes );

    return NULL;
}


int main(int argc, char *argv[]) {

    char options[64] = "--lewi --shm-key=";
    strcat(options, SHMEM_KEY);

    subprocess_descriptor_t spd;
    spd_enter_dlb(&spd);
    assert( Initialize(&spd, 111, 0, NULL, options) == DLB_SUCCESS );

    assert( pm_callback_set(&spd.pm, dlb_callback_enable_cpu,
                (dlb_callback_t)cb_enable_cpu, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd.pm, dlb_callback_disable_cpu,
                (dlb_callback_t)cb_disable_cpu, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd.pm, dlb_callback_set_num_threads,
                (dlb_callback_t)cb_set_num_threads, NULL) == DLB_SUCCESS );


    /* Test lewi_enabled */
    assert( set_lewi_enabled(&spd, false) == DLB_SUCCESS );
    assert( set_lewi_enabled(&spd, false) == DLB_NOUPDT );
    assert( lend(&spd) == DLB_ERR_DISBLD );
    assert( set_lewi_enabled(&spd, true) == DLB_SUCCESS );
    assert( set_lewi_enabled(&spd, true) == DLB_NOUPDT );

    /* Test that MPI functions invoque callbacks */
    int prev_ntimes = ntimes;
    sync_call_flags_t flags = (const sync_call_flags_t) {
        .is_mpi = true,
        .is_blocking = true,
        .is_collective = true,
        .do_lewi = true,
    };
    into_sync_call(flags);
    out_of_sync_call(flags);
    assert( prev_ntimes < ntimes );

    /* Test that MPI functions called from an observer thread do not invoque
     * callbacks */
    pthread_t observer_pthread;
    pthread_create(&observer_pthread, NULL, observer_func, &spd);
    pthread_join(observer_pthread, NULL);

    assert( Finish(&spd) == DLB_SUCCESS );

    return 0;
}
