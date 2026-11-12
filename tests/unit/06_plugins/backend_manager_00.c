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

#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/options.h"
#include "talp/backend.h"
#include "talp/backend_manager.h"

#include <assert.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

    options_t options;
    options_init(&options, NULL);
    debug_init(&options);

    // cupti
    {
        const backend_api_t *gpu_backend = talp_backend_manager_load_gpu_backend("cupti");
        assert( gpu_backend != NULL );
        assert( gpu_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( gpu_backend->struct_size == sizeof(backend_api_t) );
        assert( gpu_backend->name != NULL );
        assert( gpu_backend->capabilities.gpu );
        assert( gpu_backend->init != NULL );
        assert( gpu_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "cupti") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("cuptiSubscribe", "cupti") != NULL );
        talp_backend_manager_unload_gpu_backend();
    }

    // rocprofiler-sdk
    {
        const backend_api_t *gpu_backend = talp_backend_manager_load_gpu_backend("rocprofiler-sdk");
        assert( gpu_backend != NULL );
        assert( gpu_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( gpu_backend->struct_size == sizeof(backend_api_t) );
        assert( gpu_backend->name != NULL );
        assert( gpu_backend->capabilities.gpu );
        assert( gpu_backend->init != NULL );
        assert( gpu_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "rocprofiler-sdk") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin(
                    "rocprofiler_configure", "rocprofiler-sdk") != NULL );
        talp_backend_manager_unload_gpu_backend();
    }

    // papi
    {
        const backend_api_t *hwc_backend = talp_backend_manager_load_hwc_backend("papi");
        assert( hwc_backend != NULL );
        assert( hwc_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( hwc_backend->struct_size == sizeof(backend_api_t) );
        assert( hwc_backend->name != NULL );
        assert( hwc_backend->capabilities.hwc );
        assert( hwc_backend->init != NULL );
        assert( hwc_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "papi") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("PAPI_library_init", "papi") != NULL );
        talp_backend_manager_unload_hwc_backend();
        assert( talp_backend_manager_get_symbol_from_plugin("PAPI_library_init", "papi") == NULL );
    }

    // perf
    {
        const backend_api_t *hwc_backend = talp_backend_manager_load_hwc_backend("perf");
        assert( hwc_backend != NULL );
        assert( hwc_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( hwc_backend->struct_size == sizeof(backend_api_t) );
        assert( hwc_backend->name != NULL );
        assert( hwc_backend->capabilities.hwc );
        assert( hwc_backend->init != NULL );
        assert( hwc_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "perf") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin(
                    "talp_perf_syscall_open_default", "perf") != NULL );
        talp_backend_manager_unload_hwc_backend();
    }

    // ear
    {
        const backend_api_t *energy_backend = talp_backend_manager_load_energy_backend("ear");
        assert( energy_backend != NULL );
        assert( energy_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( energy_backend->struct_size == sizeof(backend_api_t) );
        assert( energy_backend->name != NULL );
        assert( energy_backend->capabilities.energy );
        assert( energy_backend->init != NULL );
        assert( energy_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "ear") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin(
                    "eards_connection", "ear") != NULL );
        talp_backend_manager_unload_energy_backend();
    }

    // seanerbus
    {
        const backend_api_t *energy_backend = talp_backend_manager_load_energy_backend("seanerbus");
        assert( energy_backend != NULL );
        assert( energy_backend->abi_version == DLB_BACKEND_ABI_VERSION );
        assert( energy_backend->struct_size == sizeof(backend_api_t) );
        assert( energy_backend->name != NULL );
        assert( energy_backend->capabilities.energy );
        assert( energy_backend->init != NULL );
        assert( energy_backend->finalize != NULL );
        assert( talp_backend_manager_get_symbol_from_plugin("foo", "seanerbus") == NULL );
        assert( talp_backend_manager_get_symbol_from_plugin(
                    "seanerbus_connection_create", "seanerbus") != NULL );
        talp_backend_manager_unload_energy_backend();
    }

    // GPU affinity
    {
        enum { BUF_LEN = 128 };
        char buf[BUF_LEN];
        bool full_uuid = true;
        assert( talp_backend_manager_get_gpu_affinity(buf, BUF_LEN, full_uuid) == DLB_SUCCESS );
        printf("%s\n", buf);
    }

    return 0;
}
