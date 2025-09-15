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


int main(int argc, char *argv[]) {
    char options[32] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);

    assert( DLB_Init(0, 0, options) == DLB_SUCCESS );
    dlb_monitor_t *global_region = DLB_MonitoringRegionGetGlobal();
    assert( global_region != NULL );
    assert( region_is_started(global_region) );

    assert( DLB_Finalize() == DLB_SUCCESS );
    return 0;
}
