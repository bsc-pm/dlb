/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#include "apis/dlb.h"
#include "apis/dlb_talp.h"
#include "talp/regions.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>


static void* test_observer_thread(void* arg) {
    assert( DLB_SetObserverRole(true) == DLB_SUCCESS );

    assert( DLB_MonitoringRegionStart(DLB_GLOBAL_REGION) == DLB_ERR_PERM );
    assert( DLB_MonitoringRegionStop(DLB_GLOBAL_REGION) == DLB_ERR_PERM );

    return NULL;
}

static void* test_worker_thread(void* arg) {

    dlb_monitor_t *region1 = arg;
    assert( DLB_MonitoringRegionStart(region1) == DLB_SUCCESS );
    assert( DLB_MonitoringRegionStop(region1) == DLB_SUCCESS );

    return NULL;
}


int main(int argc, char *argv[]) {
    char options[64] = "--talp --talp-papi --shm-key=";
    strcat(options, SHMEM_KEY);

    assert( DLB_Init(0, 0, options) == DLB_SUCCESS );
    dlb_monitor_t *global_region = DLB_MonitoringRegionGetGlobal();
    assert( global_region != NULL );
    assert( region_is_started(global_region) );

    /* Test that an observer thread cannot manage regions */
    {
        pthread_t thread;
        assert( pthread_create(&thread, NULL, test_observer_thread, NULL) == 0 );
        assert( pthread_join(thread, NULL) == 0 );
    }

    /* Test that a new thread could start a region created by another thread
     * (note that this messes with the PAPI hardware counters and is somewhat
     * not really supported) */
    {
        dlb_monitor_t *region1 = DLB_MonitoringRegionRegister("Region 1");
        assert( !region_is_started(region1) );
        assert( region1->num_measurements == 0 );

        pthread_t thread;
        assert( pthread_create(&thread, NULL, test_worker_thread, region1) == 0 );
        assert( pthread_join(thread, NULL) == 0 );

        assert( !region_is_started(region1) );
        assert( region1->num_measurements == 1 );
        assert( region1->useful_time > 0 );
    }

    assert( DLB_Finalize() == DLB_SUCCESS );
    return 0;
}
