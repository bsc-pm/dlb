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

#include "talp/backend_manager.h"
#include "talp/backend.h"

#include <assert.h>
#include <pthread.h>

static const backend_api_t *hwc_backend = NULL;
static __thread hwc_measurements_t hwc = {0};

static void hwc_submit(const hwc_measurements_t *raw) {
    hwc = *raw;
}

void *test_worker_thread(void *arg) {

    hwc_backend->flush();
    assert( hwc.cycles == 0 );
    assert( hwc.instructions == 0 );

    assert( hwc_backend->start() == DLB_BACKEND_SUCCESS );

    hwc_backend->flush();
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    assert( hwc_backend->stop() == DLB_BACKEND_SUCCESS );

    return NULL;
}

int main(int argc, char *argv[]) {

    const core_api_t test_core_api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(core_api_t),
        .hwc = {
            .submit_measurements = hwc_submit,
        },
    };

    hwc_backend = talp_backend_manager_load_hwc_backend("papi");
    assert( hwc_backend != NULL );

    assert( hwc_backend->init(&test_core_api) == DLB_BACKEND_SUCCESS );
    assert( hwc_backend->start() == DLB_BACKEND_SUCCESS );

    hwc_backend->flush();
    assert( hwc.cycles == 1 );
    assert( hwc.instructions == 1 );

    hwc_backend->flush();
    assert( hwc.cycles == 2 );
    assert( hwc.instructions == 2 );

    pthread_t thread;
    assert( pthread_create(&thread, NULL, test_worker_thread, NULL) == 0 );
    assert( pthread_join(thread, NULL) == 0 );

    assert( hwc_backend->stop() == DLB_BACKEND_SUCCESS );
    assert( hwc_backend->finalize() == DLB_BACKEND_SUCCESS );

    return 0;
}
