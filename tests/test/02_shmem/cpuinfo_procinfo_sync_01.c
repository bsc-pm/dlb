/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <assert.h>

// Multi init - finalize

int main( int argc, char **argv ) {
    cpu_set_t mask = { .__bits = {0} };

    assert( shmem_cpuinfo__init(42, &mask, NULL)            == DLB_SUCCESS );
    assert( shmem_procinfo__init(42, &mask, NULL, NULL)     == DLB_SUCCESS );
    assert( shmem_cpuinfo_ext__init(NULL)                   == DLB_SUCCESS );
    assert( shmem_procinfo_ext__init(NULL)                  == DLB_SUCCESS );

    assert( shmem_cpuinfo_ext__finalize()                   == DLB_SUCCESS );
    assert( shmem_procinfo_ext__finalize()                  == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(42)                     == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(42, false)             == DLB_SUCCESS );

    return 0;
}
