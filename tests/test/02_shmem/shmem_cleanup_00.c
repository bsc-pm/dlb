/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/comm_lend_light.h"
#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <stdbool.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>


void __gcov_flush() __attribute__((weak));

int main(int argc, char *argv[]) {

    pid_t pid;
    cpu_set_t process_mask;
    mu_parse_mask("0", &process_mask);

    /* Child 1 registers and dies */
    pid = fork();
    assert( pid >= 0 );
    if (pid == 0) {
        pid_t child_pid = getpid();
        assert( shmem_cpuinfo__init(child_pid, 0, &process_mask, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_procinfo__init(child_pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_async_init(child_pid, NULL, &process_mask, SHMEM_KEY) == DLB_SUCCESS );
        int barrier_id = 0;
        shmem_barrier__init(SHMEM_KEY);
        shmem_barrier__attach(barrier_id, true);
        ConfigShMem(1, 0, SHMEM_KEY);

        /* Invoke _exit so that call assert_shmem destructors are not called */
        if (__gcov_flush) __gcov_flush();
        _exit(EXIT_SUCCESS);
    }
    waitpid(pid, NULL, 0);

    /* Child 1 is dead, new child registers with the same mask */
    pid = fork();
    assert( pid >= 0 );
    if (pid == 0) {
        pid_t child_pid = getpid();
        assert( shmem_cpuinfo__init(child_pid, 0, &process_mask, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_procinfo__init(child_pid, 0, &process_mask, NULL, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_async_init(child_pid, NULL, &process_mask, SHMEM_KEY) == DLB_SUCCESS );
        int barrier_id = 0;
        shmem_barrier__init(SHMEM_KEY);
        shmem_barrier__attach(barrier_id, true);
        ConfigShMem(1, 0, SHMEM_KEY);

        /* Barrier must not block because I'm the only participant */
        shmem_barrier__barrier(barrier_id);

        assert( shmem_cpuinfo__finalize(child_pid, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_procinfo__finalize(child_pid, false, SHMEM_KEY) == DLB_SUCCESS );
        assert( shmem_async_finalize(child_pid) == DLB_SUCCESS );
        shmem_barrier__detach(barrier_id);
        shmem_barrier__finalize(SHMEM_KEY);
        finalize_comm();

        exit(EXIT_SUCCESS);
    }
    waitpid(pid, NULL, 0);

    return 0;
}
