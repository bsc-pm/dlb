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
#include <dlfcn.h>


typedef void (*rocprof_stub_call_t)(void);
static rocprof_stub_call_t rocprof_stub_init_tool = NULL;
static rocprof_stub_call_t rocprof_stub_fini_tool = NULL ;
static rocprof_stub_call_t rocprof_stub_call_runtime = NULL;
static rocprof_stub_call_t rocprof_stub_call_kernel = NULL;
static rocprof_stub_call_t rocprof_stub_call_memory_op = NULL;

void load_stub_funcs(void) {
    rocprof_stub_init_tool =
        (rocprof_stub_call_t)dlsym(RTLD_DEFAULT, "rocprof_stub_init_tool");
    assert( rocprof_stub_init_tool != NULL );

    rocprof_stub_fini_tool =
        (rocprof_stub_call_t)dlsym(RTLD_DEFAULT, "rocprof_stub_fini_tool");
    assert( rocprof_stub_fini_tool != NULL );

    rocprof_stub_call_runtime =
        (rocprof_stub_call_t)dlsym(RTLD_DEFAULT, "rocprof_stub_call_runtime");
    assert( rocprof_stub_call_runtime != NULL );

    rocprof_stub_call_kernel =
        (rocprof_stub_call_t)dlsym(RTLD_DEFAULT, "rocprof_stub_call_kernel");
    assert( rocprof_stub_call_kernel != NULL );

    rocprof_stub_call_memory_op =
        (rocprof_stub_call_t)dlsym(RTLD_DEFAULT, "rocprof_stub_call_memory_op");
    assert( rocprof_stub_call_memory_op != NULL );
}


static int num_times_entering_runtime = 0;
static int num_times_exiting_runtime = 0;
static int num_times_submit = 0;
static gpu_measurements_t gpu_measurements = {0};

static void enter_runtime(void) {
    ++num_times_entering_runtime;
}

static void exit_runtime(void) {
    ++num_times_exiting_runtime;
}

static void submit(const gpu_measurements_t *measurements) {
    ++num_times_submit;
    gpu_measurements = *measurements;
}


int main(int argc, char *argv[]) {

    const core_api_t test_core_api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(core_api_t),
        .gpu = {
            .enter_runtime = enter_runtime,
            .exit_runtime = exit_runtime,
            .submit_measurements = submit,
        },
    };

    const core_api_t test_bad_core_api = {0};

    const backend_api_t *gpu_backend = talp_backend_manager_load_gpu_backend("rocprofiler-sdk");
    assert( gpu_backend != NULL );

    /* Once the plugin is loaded, look for testing functions for triggering GPU events */
    load_stub_funcs();

    assert( gpu_backend->init(&test_bad_core_api) == DLB_BACKEND_ERROR );
    assert( gpu_backend->init(&test_core_api) == DLB_BACKEND_SUCCESS );
    assert( gpu_backend->init(&test_core_api) == DLB_BACKEND_ERROR );
    assert( gpu_backend->start() == DLB_BACKEND_SUCCESS );

    /* The plugin only does minimal initialization, it requires configuration invoked
     * from rocprofiler-sdk library */
    rocprof_stub_init_tool();

    rocprof_stub_call_runtime();
    assert( num_times_entering_runtime == 1 );
    assert( num_times_exiting_runtime == 1 );

    rocprof_stub_call_kernel();
    assert( num_times_submit == 0);
    rocprof_stub_call_kernel();
    assert( num_times_submit == 0);
    rocprof_stub_call_memory_op();
    assert( num_times_submit == 0);
    gpu_backend->flush();
    assert( num_times_submit == 1);
    assert( gpu_measurements.useful_time == 2 );
    assert( gpu_measurements.communication_time == 1 );

    assert( gpu_backend->stop() == DLB_BACKEND_SUCCESS );
    assert( gpu_backend->finalize() == DLB_BACKEND_SUCCESS );

    talp_backend_manager_unload_gpu_backend();

    return 0;
}
