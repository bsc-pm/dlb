/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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
  # This test is intentionally ignored when using BETS because we don't build
  # plugin_tests and stub libraries with Autotools. The test is executed through
  # the Meson test infrastructure instead, so no test_generator is defined here.
</testinfo>*/

#include "talp/talp_hwc.h"

#include "apis/dlb_errors.h"
#include "talp/backend.h"
#include "LB_core/spd.h"

#include <assert.h>
#include <pthread.h>


void *test_worker_thread(void *arg) {

    assert( talp_hwc_thread_init() == DLB_SUCCESS );

    hwc_measurements_t hwc;
    talp_hwc_on_state_change(TALP_STATE_DISABLED, TALP_STATE_USEFUL);
    assert( talp_hwc_collect(&hwc) == true );
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    talp_hwc_thread_finalize();

    return NULL;
}

int main(int argc, char *argv[]) {

    hwc_measurements_t hwc;

    subprocess_descriptor_t spd = {0};
    spd_enter_dlb(&spd);

    assert( talp_hwc_init(&spd) == DLB_SUCCESS );

    /* Initial state is disabled, no HWC should be returned */
    assert( talp_hwc_collect(&hwc) == false );
    assert( hwc.cycles == 0 );
    assert( hwc.instructions == 0 );

    /* Check that counters are incremented after switching to USEFUL */
    talp_hwc_on_state_change(TALP_STATE_DISABLED, TALP_STATE_USEFUL);
    assert( talp_hwc_collect(&hwc) == true );
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    /* A new collect causes a new flush, but only the difference is collected */
    assert( talp_hwc_collect(&hwc) == true );
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    /* Right now we don't support multiple state changes between readings,
     * changing state and collecting metrics is not an atomic operation.
     * We may revisit it in the future. */
    talp_hwc_on_state_change(TALP_STATE_USEFUL, TALP_STATE_NOT_USEFUL_MPI);
    talp_hwc_on_state_change(TALP_STATE_NOT_USEFUL_MPI, TALP_STATE_USEFUL);
    talp_hwc_on_state_change(TALP_STATE_USEFUL, TALP_STATE_NOT_USEFUL_GPU);
    talp_hwc_on_state_change(TALP_STATE_NOT_USEFUL_GPU, TALP_STATE_USEFUL);
    assert( talp_hwc_collect(&hwc) == true );
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    /* Test threads */
    pthread_t thread;
    assert( pthread_create(&thread, NULL, test_worker_thread, NULL) == 0 );
    assert( pthread_join(thread, NULL) == 0 );

    talp_hwc_finalize();

    return 0;
}
