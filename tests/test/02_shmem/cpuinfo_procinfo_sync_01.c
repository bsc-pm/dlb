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

#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <assert.h>


int main( int argc, char **argv ) {
    /* This test needs at least room for 4 CPUs */
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    /* Multi init - finalize */
    cpu_set_t empty_mask;
    CPU_ZERO(&empty_mask);
    assert( shmem_cpuinfo__init(42, 0, &empty_mask, SHMEM_KEY)        == DLB_SUCCESS );
    assert( shmem_procinfo__init(42, 0, &empty_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
    assert( shmem_cpuinfo_ext__init(SHMEM_KEY)                        == DLB_SUCCESS );
    assert( shmem_procinfo_ext__init(SHMEM_KEY)                       == DLB_SUCCESS );

    assert( shmem_cpuinfo_ext__finalize()                       == DLB_SUCCESS );
    assert( shmem_procinfo_ext__finalize()                      == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(42, SHMEM_KEY)              == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(42, false, SHMEM_KEY)      == DLB_SUCCESS );

    /* CPU stealing */
    {
        /* Process 1 initializes with mask 0-3 */
        pid_t p1_pid = 111;
        cpu_set_t p1_mask;
        mu_parse_mask("0-3", &p1_mask);
        assert( shmem_procinfo__init(p1_pid, 0, &p1_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_cpuinfo__init(p1_pid, 0, &p1_mask, SHMEM_KEY) == DLB_SUCCESS );

        /* Process 2 enters and steals CPU 3 */
        pid_t p2_pid = 222;
        cpu_set_t p2_mask;
        mu_parse_mask("3", &p2_mask);
        /* DLB_DROM_PreInit: */
        assert( shmem_procinfo_ext__preinit(p2_pid, &p2_mask, DLB_STEAL_CPUS) == DLB_SUCCESS );
        assert( shmem_cpuinfo_ext__preinit(p2_pid, &p2_mask, DLB_STEAL_CPUS)  == DLB_SUCCESS );
        /* DLB_Init: (with p2_pid as preinit_pid) */
        cpu_set_t new_mask;
        assert( shmem_procinfo__init(p2_pid, p2_pid, &p2_mask, &new_mask, SHMEM_KEY) == DLB_NOTED );
        assert( CPU_EQUAL(&new_mask, &p2_mask) );
        assert( shmem_cpuinfo__init(p2_pid, p2_pid, &p2_mask, SHMEM_KEY) == DLB_SUCCESS );

        /* Process 1 polls DROM */
        assert( shmem_procinfo__polldrom(p1_pid, NULL, &new_mask) == DLB_SUCCESS );
        pid_t new_guests[SYS_SIZE];
        shmem_cpuinfo__update_ownership(p1_pid, &new_mask, new_guests);
        assert( new_guests[3] == p2_pid );
        assert( new_guests[0] == -1 && new_guests[1] == -1 && new_guests[2] == -1 );

        /* All CPUs are correctly guested */
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 0) == DLB_SUCCESS );
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 1) == DLB_SUCCESS );
        assert( shmem_cpuinfo__check_cpu_availability(p1_pid, 2) == DLB_SUCCESS );
        assert( shmem_cpuinfo__check_cpu_availability(p2_pid, 3) == DLB_SUCCESS );

        /* Finalize shmems */
        assert( shmem_cpuinfo__finalize(p2_pid, SHMEM_KEY)              == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(p2_pid, false, SHMEM_KEY)      == DLB_SUCCESS );
        assert( shmem_cpuinfo__finalize(p1_pid, SHMEM_KEY)              == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(p1_pid, false, SHMEM_KEY)      == DLB_SUCCESS );
    }

    return 0;
}
