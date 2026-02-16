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

#include "talp/talp_gpu.h"

#include "apis/dlb_errors.h"
#include "apis/dlb_talp.h"
#include "talp/backend.h"
#include "talp/talp.h"
#include "LB_core/spd.h"

#include <assert.h>
#include <dlfcn.h>


int main(int argc, char *argv[]) {

    /* Initialize TALP with empty options so that regions and samples
     * are initialized, then manage GPU component from here */
    subprocess_descriptor_t spd = {0};
    spd_enter_dlb(&spd);
    talp_init(&spd);

    /* Get global monitor */
    talp_info_t *talp_info = spd.talp_info;
    const dlb_monitor_t *global_monitor = talp_info->monitor;
    assert( global_monitor->num_measurements == 0 );

    /* Get thread sample */
    const talp_sample_t* thread_sample = talp_get_thread_sample(&spd);
    assert( thread_sample->stats.num_gpu_runtime_calls == 0 );

    assert( talp_gpu_init(&spd) == DLB_SUCCESS );

    /* Once the plugin is loaded, look for testing functions for triggering GPU events */
    typedef void (*cupti_stub_call_t)(void);
    cupti_stub_call_t cupti_stub_call_runtime = (cupti_stub_call_t)
        dlsym(RTLD_DEFAULT, "cupti_stub_call_runtime");
    assert( cupti_stub_call_runtime != NULL );
    cupti_stub_call_t cupti_stub_call_kernel = (cupti_stub_call_t)
        dlsym(RTLD_DEFAULT, "cupti_stub_call_kernel");
    assert( cupti_stub_call_kernel != NULL );
    cupti_stub_call_t cupti_stub_call_memory_op = (cupti_stub_call_t)
        dlsym(RTLD_DEFAULT, "cupti_stub_call_memory_op");
    assert( cupti_stub_call_memory_op != NULL );

    /* Call runtime */
    cupti_stub_call_runtime();
    assert( thread_sample->stats.num_gpu_runtime_calls == 1 );

    /* Call kernel */
    gpu_measurements_t measurements = {0};
    talp_gpu_collect(&measurements);
    assert( measurements.useful_time == 0 );
    assert( measurements.communication_time == 0 );

    cupti_stub_call_kernel();
    talp_gpu_collect(&measurements);
    assert( measurements.useful_time == 1 );
    assert( measurements.communication_time == 0 );

    /* Call memory op */
    cupti_stub_call_memory_op();
    cupti_stub_call_memory_op();
    talp_gpu_collect(&measurements);
    assert( measurements.useful_time == 0 );
    assert( measurements.communication_time == 2 );

    talp_gpu_finalize();

    talp_finalize(&spd);

    return 0;
}
