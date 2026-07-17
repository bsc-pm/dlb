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
#include "mngo/mngo_resources.h"
#include "support/mask_utils.h"
#include "talp/talp_mpi.h"
#include "test_process.h"
#include "unique_shmem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* TLS global variable to store the spd pointer */
extern __thread subprocess_descriptor_t *thread_spd;

static char options[128];

cpu_set_t* get_mask_from_str(const char *mask) {
    cpu_set_t *mask_ptr = calloc(1, sizeof(cpu_set_t));
    mu_parse_mask(mask, mask_ptr);
    return mask_ptr;
} 

void initialize(size_t id, const char *mask, subprocess_descriptor_t *spd) {
    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    cpu_set_t *process_mask = get_mask_from_str(mask);

    spd->id = id;
    thread_spd = spd;

    assert(Initialize(spd, id, 2, process_mask, options) == DLB_SUCCESS);
    mngo_init(spd);

    free(process_mask);

    usleep(500); // Allow some time for the other processes to initialize their memory
}

void finalize(subprocess_descriptor_t *spd) {
    mngo_fini(spd);
    assert(Finish(spd) == DLB_SUCCESS);
}

void test__mngo(pid_t id, const char* mask,  size_t load) {

    subprocess_descriptor_t spd;
    initialize(id, mask, &spd);
    talp_mpi_init(&spd);

    {
        dlb_mngo_region_t *region = mngo_region_register(&spd, "test");
        assert(region != NULL);
    }

    mngo_info_t *mngo_info = spd.mngo_info;
    queue_iter_head2tail_t iter = queue__into_head2tail_iter(mngo_info->region_handler.regions);
    dlb_mngo_region_t **monitor_from_queue = queue_iter__get_nth(&iter, 0);

    
    dlb_mngo_region_t *region = mngo_region_register(&spd, "test");
    assert(region->region_info.talp.monitor == (*monitor_from_queue)->region_info.talp.monitor);

    assert(mngo_region_start(&spd, region) == DLB_SUCCESS
           && "Expected to close the region normally, before imbalanced load");
    usleep(load * 1000);
    assert(mngo_region_stop(&spd, region) == DLB_SUCCESS
           && "Expected to close the region normally, after imbalanced load");

    if (spd.options.drom) {
        // For now: DROM takes preference, so if drom and lewi are enabled we sould see some change
        //  in DROM

        cpu_set_t *original_mask = get_mask_from_str(mask);
        // Check that the mask has changed
        assert(memcmp(&region->region_info.state.drom_mask, original_mask, sizeof(cpu_set_t)) != 0);
        free(original_mask);
    } else if (spd.options.lewi) {
        assert(region->region_info.state.lewi_on
               && "lewi should be ON after load imbalance detected");
    }

    assert(mngo_region_start(&spd, region) == DLB_SUCCESS
           && "Expected to start the region normally, before balanced load");
    usleep(400 * 1000);
    assert(mngo_region_stop(&spd, region) == DLB_SUCCESS
           && "Expected to close the region normally, after balanced load");

    {
        dlb_mngo_region_t *region_not_top = mngo_region_register(&spd, "test-not-top");
        assert(region != NULL);

        // Test close not open region
        assert(mngo_region_stop(&spd, region_not_top) == DLB_NOUPDT
               && "Expected the region to not be open");

        // Test open already open region
        assert(mngo_region_start(&spd, region_not_top) == DLB_SUCCESS
               && "Expected the region to not be open");
        assert(mngo_region_start(&spd, region_not_top) == DLB_NOUPDT
               && "Expected the region to be already open");

        assert(mngo_region_stop(&spd, region_not_top) == DLB_SUCCESS
               && "Expected to close the region normally as its open");
    }

    talp_mpi_finalize(&spd);
    finalize(&spd);
}

int main(int argc, char *argv[]) {
    enum { SYS_SIZE = 128 };
    mu_testing_set_sys_size(SYS_SIZE);

    // Test mngo regions with DROM
    strcpy(options, "--mngo --mngo-mode=regions --talp --drom --verbose=mngo --shm-key=");
    strcat(options, SHMEM_KEY);
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(test__mngo(1, "0-63", 1));
    FORK(test__mngo(2, "64-127", 200));
    WAITALL;

    // Test mngo regions with LeWI
    strcpy(options, "--mngo --mngo-mode=regions --talp --lewi --verbose=mngo --shm-key=");
    strcat(options, SHMEM_KEY);
    test_shmem_mngo__modify_barrier_participants(2);
    FORK(test__mngo(1, "0-63", 1));
    FORK(test__mngo(2, "64-127", 200));
    WAITALL;

    return 0;    
}
