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
#include "test_process.h"
#include "dlb_errors.h"
#include "mngo/mngo.h"
#include "mngo/mngo_resources.h"
#include "support/mask_utils.h"
#include "unique_shmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* TLS global variable to store the spd pointer */
extern __thread subprocess_descriptor_t *thread_spd;

static char options[128];

extern int _mpis_per_node;

void initialize(size_t id, const char *mask, subprocess_descriptor_t *spd) {
    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    cpu_set_t process_mask;
    mu_parse_mask(mask, &process_mask);
    spd->id = id;
    thread_spd = spd;

    assert(Initialize(spd, id, 2, &process_mask, options) == DLB_SUCCESS);
    mngo_init(spd);

    usleep(500); // Allow some time for the other processes to initialize their memory
}

void finalize(subprocess_descriptor_t *spd) {
    mngo_fini(spd);
    assert(Finish(spd) == DLB_SUCCESS);
}

int test__mngo_state_change_drom_mask(const char *original_mask, const char *new_mask, size_t id) {

    subprocess_descriptor_t spd;
    initialize(id, original_mask, &spd);

    cpu_set_t new_process_mask;
    mu_parse_mask(new_mask, &new_process_mask);
    mngo_state_t new_state = {
        .lewi_on = false,
    };
    memcpy(&new_state.drom_mask, &new_process_mask, sizeof(cpu_set_t));
    
    int error = mngo_state__load(&spd, &new_state);

    shmem_mngo_barrier_wait();
    finalize(&spd);

    return error;
}

int test__mngo_state_lewi(const char *mask, size_t id) {
    subprocess_descriptor_t spd;
    initialize(id, mask, &spd);

    mngo_state_t new_state = {
        .lewi_on = true,
    };

    assert(!spd.lewi_enabled);
    int error = mngo_state__load(&spd, &new_state);
    assert(spd.lewi_enabled);

    shmem_mngo_barrier_wait();
    finalize(&spd);

    return error;
}

int main(int argc, char *argv[]) {
    enum { SYS_SIZE = 4 };
    mu_testing_set_sys_size(SYS_SIZE);

    fprintf(stderr, "Testing normal ussage of `mngo_state__load`:\n");
    strcpy(options, "--mngo --mngo-mode=regions --drom --verbose=mngo,drom,barrier --shm-key=");
    strcat(options, SHMEM_KEY);
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(test__mngo_state_change_drom_mask("0-1", "0-1,3", 1));
    FORK(test__mngo_state_change_drom_mask("2-3", "2", 2));
    WAITALL;

    fprintf(stderr, "Testing normal ussage of `mngo_state__load` (verbose):\n");
    strcpy(options, "--mngo --mngo-mode=regions --drom --verbose=mngo,drom,barrier --shm-key=");
    strcat(options, SHMEM_KEY);
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(test__mngo_state_change_drom_mask("0-1", "0-1,3", 1));
    FORK(test__mngo_state_change_drom_mask("2-3", "2", 2));
    WAITALL;

    #if 0
    fprintf(stderr, "Testing bad ussage of `mngo_state__load` (verbose):\n");
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(assert(test__mngo_state_change_drom_mask("0-1", "0-1,3", 1) == DLB_ERR_PERM));
    FORK(assert(test__mngo_state_change_drom_mask("2-3", "2-3", 2) == DLB_SUCCESS));
    WAITALL;
    #endif

    strcpy(options, "--mngo --mngo-mode=regions --lewi --shm-key=");
    strcat(options, SHMEM_KEY);
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(test__mngo_state_lewi("0-1", 1));
    FORK(test__mngo_state_lewi("2-3", 2));
    WAITALL;
    return 0;    
}
