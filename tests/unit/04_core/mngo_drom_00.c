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
    test_generator="gens/basic-generator"
</testinfo>*/


#include "LB_comm/shmem_mngo.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "dlb_errors.h"
#include "mngo/mngo.h"
#include "mngo/mngo_drom.h"
#include "support/mask_utils.h"
#include "unique_shmem.h"
#include "test_process.h"
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* TLS global variable to store the spd pointer */
extern __thread subprocess_descriptor_t *thread_spd;

static char options[128];

int test__mngo_drom_balance(const char *original_mask, size_t id, float self_pe, float node_pe, int expected_cpu_count) {

    int error = DLB_SUCCESS;

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    cpu_set_t process_mask;
    mu_parse_mask(original_mask, &process_mask);

    subprocess_descriptor_t spd = {.id = id};
    thread_spd = &spd;

    assert(Initialize(&spd, id, 2, &process_mask, options) == DLB_SUCCESS);
    mngo_init(&spd);

    cpu_set_t new_cpu_mask;
    mngo_drom__balance(&spd, self_pe, node_pe, &new_cpu_mask);

    shmem_mngo_barrier_wait();
    mngo_fini(&spd);
    assert(Finish(&spd) == DLB_SUCCESS);

    return error;
}

int main(int argc, char *argv[]) {
    enum { SYS_SIZE = 4 };
    mu_testing_set_sys_size(SYS_SIZE);

    fprintf(stderr, "Testing normal ussage of `mngo_state__load`:\n");

    strcpy(options, "--mngo --mngo-mode=regions --drom --shm-key=");
    strcat(options, SHMEM_KEY);

    // Balanced test
    FORK(test__mngo_drom_balance("0-1", 1, 0.50, 0.50, 2));
    FORK(test__mngo_drom_balance("2-3", 2, 0.50, 0.50, 2));
    WAITALL;

    // Imbalanced test
    FORK(test__mngo_drom_balance("0-1", 1, 0.20, 0.60, 1));
    FORK(test__mngo_drom_balance("2-3", 2, 1.00, 0.60, 3));
    WAITALL;

    return 0;    
}
