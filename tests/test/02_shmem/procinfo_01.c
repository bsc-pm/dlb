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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Basic checks with 2+ sub-processes

int main( int argc, char **argv ) {
    pid_t pid = getpid();
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    // Check permission error with two subprocesses sharing mask
    {
        assert( shmem_procinfo__init(pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
        if (num_cpus > 1) {
            assert( shmem_procinfo__init(pid+1, 0, &process_mask, NULL, SHMEM_KEY)
                    == DLB_ERR_PERM );
        }
        assert( shmem_procinfo__finalize(pid, false, SHMEM_KEY) == DLB_SUCCESS );

        // Check that the shared memory has been finalized
        assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    }

    // Check shared memory capacity
    {
        // This loop should completely fill the shared memory
        // WARNING: this test only works if proces_mask is 0..N-1
        if (CPU_COUNT(&process_mask) == mu_get_system_size()) {
            int cpuid;
            for (cpuid=0; cpuid<num_cpus; ++cpuid) {
                CPU_ZERO(&process_mask);
                CPU_SET(cpuid, &process_mask);
                assert( shmem_procinfo__init(pid+cpuid, 0, &process_mask, NULL, SHMEM_KEY)
                        == DLB_SUCCESS );
            }

            // Another initialization should return error
            CPU_ZERO(&process_mask);
            assert( shmem_procinfo__init(pid+cpuid, 0, &process_mask, NULL, SHMEM_KEY)
                    == DLB_ERR_NOMEM );

            // Finalize all
            for (cpuid=0; cpuid<num_cpus; ++cpuid) {
                assert( shmem_procinfo__finalize(pid+cpuid, false, SHMEM_KEY) == DLB_SUCCESS );
            }

            // Check that the shared memory has been finalized
            assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
        }
    }

    // Check that each subprocess finalizes its own part of the shared memory
    {
        // Initialize as many subprocess as number of CPUs
        int i;
        for (i=0; i<num_cpus; ++i) {
            CPU_ZERO(&process_mask);
            CPU_SET(i, &process_mask);
            assert( shmem_procinfo__init(i+1, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
        }

        // subprocess 1 tries to finalize as many times as number of CPUs
        assert( shmem_procinfo__finalize(1, false, SHMEM_KEY) == DLB_SUCCESS );
        for (i=1; i<num_cpus; ++i) {
            assert( shmem_procinfo__finalize(1, false, SHMEM_KEY) == DLB_ERR_NOPROC );
        }

        // rest of subprocesses should finalize correctly
        for (i=1; i<num_cpus; ++i) {
            assert( shmem_procinfo__finalize(i+1, false, SHMEM_KEY) == DLB_SUCCESS );
        }

        // Check that the shared memory has been finalized
        assert( shmem_procinfo__getprocessmask(pid, NULL, DLB_SYNC_QUERY) == DLB_ERR_NOSHMEM );
    }

    // Change CPU ownership
    {
        if (num_cpus >= 4) {
            dlb_drom_flags_t flags = 0;
            cpu_set_t p1_mask, p2_mask, mask;
            mu_parse_mask("0-1", &p1_mask);
            mu_parse_mask("2-3", &p2_mask);

            assert( shmem_procinfo__init(111, 0, &p1_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
            assert( shmem_procinfo__init(222, 0, &p2_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );

            // remove CPU 1 from subprocess 1
            mu_parse_mask("0", &mask);
            cpu_set_t free_mask;
            assert( shmem_procinfo__setprocessmask(111, &mask, flags, &free_mask) == DLB_SUCCESS );

            // check the free mask
            assert( CPU_COUNT(&free_mask) == 1 );
            assert( CPU_ISSET(1, &free_mask) && !CPU_ISSET(0, &free_mask) ); 

            assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
            assert( CPU_COUNT(&mask) == 1 && CPU_ISSET(0, &mask) );

            // removing CPU 0 from subprocess 1 is also allowed
            CPU_ZERO(&mask);
            assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_SUCCESS );
            assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
            assert( CPU_COUNT(&mask) == 0 );

            // test CPU stealing without return_stolen
            {
                // set mask 0-2 to subprocess 1
                mu_parse_mask("0-2", &mask);
                assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has now only CPU 3
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 1 && CPU_ISSET(3, &mask) );
                // check that subprocess 1 has CPUs 0-2
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 3
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) && CPU_ISSET(2, &mask) );

                // set original mask to subprocess 1 (WITHOUT return_stolen)
                assert( shmem_procinfo__setprocessmask(111, &p1_mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has not been updated
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_NOUPDT );
                // check that subprocess 1 has CPUs 0-1
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) );

                // manually recover CPUs for subprocess 2
                assert( shmem_procinfo_ext__recover_stolen_cpus(222) == DLB_SUCCESS );
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(2, &mask) && CPU_ISSET(3, &mask) );

                // check that there are not stolen CPUs
                assert( shmem_procinfo_ext__recover_stolen_cpus(222) == DLB_NOUPDT );
            }

            // test CPU stealing with return_stolen
            {
                // set mask 0-2 to subprocess 1 (again)
                mu_parse_mask("0-2", &mask);
                assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has now only CPU 3
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 1 && CPU_ISSET(3, &mask) );
                // check that subprocess 1 has CPUs 0-2
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 3
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) && CPU_ISSET(2, &mask) );

                // set original mask to subprocess 1 (WITH return_stolen)
                flags |= DLB_RETURN_STOLEN;
                assert( shmem_procinfo__setprocessmask(111, &p1_mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has CPUs 2-3
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(2, &mask) && CPU_ISSET(3, &mask) );
                // check that subprocess 1 has CPUs 0-1
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) );

                // check that there are not stolen CPUs
                assert( shmem_procinfo_ext__recover_stolen_cpus(222) == DLB_NOUPDT );
            }

            // test stealing all CPUs
            {
                // subprocess 1 gets all CPUs
                mu_parse_mask("0-3", &mask);
                assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has empty mask
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 0 );
                // check that subprocess 1 has CPUs 0-3
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 4
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask)
                        && CPU_ISSET(2, &mask) && CPU_ISSET(3, &mask) );

                // set original mask to subprocess 1 (WITH return_stolen)
                flags |= DLB_RETURN_STOLEN;
                assert( shmem_procinfo__setprocessmask(111, &p1_mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has CPUs 2-3
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(2, &mask) && CPU_ISSET(3, &mask) );
                // check that subprocess 1 has CPUs 0-1
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) );

                // check that there are not stolen CPUs
                assert( shmem_procinfo_ext__recover_stolen_cpus(222) == DLB_NOUPDT );
            }

            // test setting same mask as future
            {
                flags = 0;

                // subprocess 1 steals one CPU from suprocess 2
                mu_parse_mask("0-1,3", &mask);
                assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_SUCCESS );

                // subprocess 2 changes mask to "2" before resolving previous change
                mu_parse_mask("2", &mask);
                assert( shmem_procinfo__setprocessmask(222, &mask, flags, NULL) == DLB_SUCCESS );

                // check that subprocess 1 has CPUs 0-3
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 3
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask)
                        && CPU_ISSET(3, &mask) );

                // check that subprocess 2 has CPU 2
                assert( shmem_procinfo__getprocessmask(222, &mask, flags) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 1 && CPU_ISSET(2, &mask) );

                // check that both processes are not dirty
                assert( shmem_procinfo__polldrom(111, NULL, NULL) == DLB_NOUPDT );
                assert( shmem_procinfo__polldrom(222, NULL, NULL) == DLB_NOUPDT );

                // set original mask to subprocess 1 (WITH return_stolen)
                flags |= DLB_RETURN_STOLEN;
                assert( shmem_procinfo__setprocessmask(111, &p1_mask, flags, NULL) == DLB_SUCCESS );
                // check that subprocess 2 has CPUs 2-3
                assert( shmem_procinfo__polldrom(222, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(2, &mask) && CPU_ISSET(3, &mask) );
                // check that subprocess 1 has CPUs 0-1
                assert( shmem_procinfo__polldrom(111, NULL, &mask) == DLB_SUCCESS );
                assert( CPU_COUNT(&mask) == 2
                        && CPU_ISSET(0, &mask) && CPU_ISSET(1, &mask) );

                // check that there are not stolen CPUs
                assert( shmem_procinfo_ext__recover_stolen_cpus(222) == DLB_NOUPDT );
            }

            assert( shmem_procinfo__finalize(111, false, SHMEM_KEY) == DLB_SUCCESS );
            assert( shmem_procinfo__finalize(222, false, SHMEM_KEY) == DLB_SUCCESS );
        }
    }

    // Check incompatible shmem options
    {
        assert( shmem_procinfo__init_with_cpu_sharing(111, 0, &process_mask, SHMEM_KEY)
                == DLB_SUCCESS );

        dlb_drom_flags_t flags = 0;
        cpu_set_t mask;
        mu_parse_mask("0", &mask);
        assert( shmem_procinfo__setprocessmask(111, &mask, flags, NULL) == DLB_ERR_NOCOMP );

        assert( shmem_procinfo__finalize(111, false, SHMEM_KEY) == DLB_SUCCESS );
    }


    return 0;
}
