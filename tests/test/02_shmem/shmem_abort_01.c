/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/* Test that shared memories are cleaned when the process aborts, even if the
 * process has initializes multiple subprocesses.
 * The fork is necessary to call the assert_noshm in the destructor
 */

#include "assert_noshm.h"

#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_core/spd.h"
#include "support/debug.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>

void __gcov_flush() __attribute__((weak));


int main(int argc, char **argv) {
    // Create a child process
    pid_t pid = fork();
    assert( pid >= 0 );
    if (pid == 0) {

        enum { SYS_SIZE = 4 };
        mu_testing_set_sys_size(SYS_SIZE);

        /* spd1 */
        subprocess_descriptor_t spd1;
        spd1.id = 111;
        spd1.options.shm_key[0] = '\0';
        mu_parse_mask("0", &spd1.process_mask);
        spd_register(&spd1);
        assert( shmem_cpuinfo__init(spd1.id, &spd1.process_mask, spd1.options.shm_key)
                == DLB_SUCCESS );
        assert( shmem_procinfo__init(spd1.id, &spd1.process_mask, NULL, spd1.options.shm_key)
                == DLB_SUCCESS );
        assert( shmem_async_init(spd1.id, NULL, &spd1.process_mask, spd1.options.shm_key)
                == DLB_SUCCESS );

        /* spd2 */
        subprocess_descriptor_t spd2;
        spd2.id = 222;
        spd2.options.shm_key[0] = '\0';
        mu_parse_mask("1", &spd2.process_mask);
        spd_register(&spd2);
        assert( shmem_cpuinfo__init(spd2.id, &spd2.process_mask, spd2.options.shm_key)
                == DLB_SUCCESS );

        /* spd3 */
        subprocess_descriptor_t spd3;
        spd3.id = 333;
        spd3.options.shm_key[0] = '\0';
        mu_parse_mask("2", &spd3.process_mask);
        spd_register(&spd3);
        assert( shmem_cpuinfo__init(spd3.id, &spd3.process_mask, spd3.options.shm_key)
                == DLB_SUCCESS );

        /* spd4 */
        subprocess_descriptor_t spd4;
        spd4.id = 444;
        spd4.options.shm_key[0] = '\0';
        mu_parse_mask("3", &spd4.process_mask);
        spd_register(&spd4);
        assert( shmem_cpuinfo__init(spd4.id, &spd4.process_mask, spd4.options.shm_key)
                == DLB_SUCCESS );

        if (__gcov_flush) __gcov_flush();

        fatal("This fatal should clean shmems");
    }

    // Wait child and end execution correctly in order to call destructors
    int wstatus;
    assert( waitpid(pid, &wstatus, 0) > 0 );
    assert( WIFSIGNALED(wstatus) && WTERMSIG(wstatus) == SIGABRT );

    return 0;
}
