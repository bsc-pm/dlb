/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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
#include "LB_comm/shmem_mngo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <assert.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void assert_reduce(mngo_actions_t *a1, mngo_actions_t *a2) {

    /* We only check the shared of the a1 because they come from different
     * processes and the a2 does not contain the updated information.
     */
    assert( (a1->self_lewi_start | a2->self_lewi_start)
           == a1->shared_lewi_start);

    assert( (a1->self_lewi_stop | a2->self_lewi_stop)
           == a1->shared_lewi_stop);

    assert( (a1->self_drom_lb_in | a2->self_drom_lb_in)
           == a1->shared_drom_lb_in);

    assert( (a1->self_drom_lb_out | a2->self_drom_lb_out)
           == a1->shared_drom_lb_out);
}

int main(int argc, char *argv[]) {

    enum { SYS_SIZE = 2 };

    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);



    /* Reduce actions */
    {
        mngo_actions_t actions_1 = {
            .self_lewi_start = false,
            .self_lewi_stop = false,
            .self_drom_lb_in = true,
            .self_drom_lb_out = false,
            .self_drom_cpu_change = 0,
            .self_drom_next_mask = {},
            .shared_lewi_start = false,
            .shared_lewi_stop = false,
            .shared_drom_lb_in = false,
            .shared_drom_lb_out = false,
        };

        mngo_actions_t actions_2 = {
            .self_lewi_start = false,
            .self_lewi_stop = false,
            .self_drom_lb_in = false,
            .self_drom_lb_out = true,
            .self_drom_cpu_change = 0,
            .self_drom_next_mask = {},
            .shared_lewi_start = false,
            .shared_lewi_stop = false,
            .shared_drom_lb_in = false,
            .shared_drom_lb_out = false,
        };

        int pid = fork();
        if (pid) {
            pid_t pid1 = 1;
            size_t manager_id_1;

            /* Initialize shmem mngo (first instance) */
            assert(shmem_mngo_init(SHMEM_KEY, pid1, &manager_id_1) == DLB_SUCCESS);
            shmem_mngo_allreduce_actions(manager_id_1, &actions_1);
            assert(shmem_mngo_fini(manager_id_1) == DLB_SUCCESS);
            waitpid(pid, NULL, 0);
        } else {
            pid_t pid2 = 2;
            size_t manager_id_2;

            /* Initialize shmem mngo (first instance) */
            assert(shmem_mngo_init(SHMEM_KEY, pid2, &manager_id_2) == DLB_SUCCESS);
            shmem_mngo_allreduce_actions(manager_id_2, &actions_2);
            assert(shmem_mngo_fini(manager_id_2) == DLB_SUCCESS);
            _exit(0);
        }

        assert_reduce(&actions_1, &actions_2);
    }
    {
        mngo_actions_t actions_1 = {
            .self_lewi_start = false,
            .self_lewi_stop = false,
            .self_drom_lb_in = true,
            .self_drom_lb_out = false,
            .self_drom_cpu_change = 0,
            .self_drom_next_mask = {},
            .shared_lewi_start = false,
            .shared_lewi_stop = false,
            .shared_drom_lb_in = false,
            .shared_drom_lb_out = false,
        };

        mngo_actions_t actions_2 = {
            .self_lewi_start = false,
            .self_lewi_stop = false,
            .self_drom_lb_in = false,
            .self_drom_lb_out = false,
            .self_drom_cpu_change = 0,
            .self_drom_next_mask = {},
            .shared_lewi_start = false,
            .shared_lewi_stop = false,
            .shared_drom_lb_in = false,
            .shared_drom_lb_out = false,
        };

        int pid = fork();
        if (pid) {
            pid_t pid1 = 1;
            size_t manager_id_1;

            /* Initialize shmem mngo (first instance) */
            assert(shmem_mngo_init(SHMEM_KEY, pid1, &manager_id_1) == DLB_SUCCESS);
            shmem_mngo_allreduce_actions(manager_id_1, &actions_1);
            assert(shmem_mngo_fini(manager_id_1) == DLB_SUCCESS);
            waitpid(pid, NULL, 0);
        } else {
            pid_t pid2 = 2;
            size_t manager_id_2;

            /* Initialize shmem mngo (first instance) */
            assert(shmem_mngo_init(SHMEM_KEY, pid2, &manager_id_2) == DLB_SUCCESS);
            shmem_mngo_allreduce_actions(manager_id_2, &actions_2);
            assert(shmem_mngo_fini(manager_id_2) == DLB_SUCCESS);
            _exit(0);
        }

        assert_reduce(&actions_1, &actions_2);
    }
}
