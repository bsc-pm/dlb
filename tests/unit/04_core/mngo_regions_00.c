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

#include <assert.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "dlb_errors.h"
#include "dlb_types.h"
#include "mngo/mngo_regions.h"
#include "support/mask_utils.h"

#define TEST_FAIL(...) do { \
  fprintf(stderr, "[FAIL] %s: %d in %s: ", __FILE__, __LINE__, __func__); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
} while (0)

const char* region_names[4] = {
  "0",  
  "1",  
  "2",  
  "3",  
};

#define NUM_TEST_REGIONS 4

int main(int argc, char *argv[]) {
    mngo_regions_manager_t region_manager;

    cpu_set_t process_mask;
    mu_parse_mask("0-3", &process_mask);

    mngo_state_t default_state = (const mngo_state_t) {
        .lewi_on      = false,
    };
    memcpy(&default_state.drom_mask, &process_mask, sizeof(cpu_set_t));

    mngo_regions__manager_init(&region_manager, &default_state);

    mngo_state_t *current_state = mngo_regions__get_current_resources(&region_manager);
    assert(default_state.lewi_on == current_state->lewi_on);
    assert(memcmp(&default_state.drom_mask, &current_state->drom_mask, sizeof(cpu_set_t)) == 0);

    for (int i = 0; i < NUM_TEST_REGIONS; i++) {
        dlb_mngo_region_t *region = mngo_regions__alloc(&region_manager);
        region->name = region_names[i];
    }

    // Check check on empty stack returns false
    assert(!mngo_regions__check_top(
        &region_manager,
        mngo_regions__find(&region_manager, region_names[0])));

    /**
      * Check that regions survive reallocation.
      *
      * Basically the point of this test is to check if the region_0 pointer is still valid after
      * the reallocation that happens at the alloc. (Note: the reallocation happens because the
      * queue_t capacity grows by powers of four, if this ever changed this test is no longer valid)
      */
    dlb_mngo_region_t * region_0 = mngo_regions__find(&region_manager, region_names[0]);

    assert(DLB_SUCCESS == mngo_regions__push_active(&region_manager, region_0));
    assert(mngo_regions__check_top(&region_manager, region_0));

    // This alloc will create an allocation as we now had 4 regions
    dlb_mngo_region_t *dummy_region = mngo_regions__alloc(&region_manager);
    dummy_region->name = "dummy";

    assert(mngo_regions__check_top(&region_manager, region_0));
    
    /**
      * Check normal ussage
      */

    // Check push active regions
    for (int i = 0; i < NUM_TEST_REGIONS; i++) {
        dlb_mngo_region_t *region = mngo_regions__find(&region_manager, region_names[i]);
        assert(DLB_SUCCESS == mngo_regions__push_active(&region_manager, region));
        assert(mngo_regions__check_top(&region_manager, region));
    }

    // Check pop active regions
    for (int i = NUM_TEST_REGIONS-1; i >= 0; i--) {
        dlb_mngo_region_t *region = mngo_regions__find(&region_manager, region_names[i]);
        assert(mngo_regions__check_top(&region_manager, region));
        assert(DLB_SUCCESS == mngo_regions__pop_active(&region_manager, region));
    }

    // Make the stack [0, 1]
    assert(DLB_SUCCESS == mngo_regions__push_active(
        &region_manager,
        mngo_regions__find(&region_manager, region_names[0])));
    assert(DLB_SUCCESS == mngo_regions__push_active(
        &region_manager,
        mngo_regions__find(&region_manager, region_names[1])));

    // Check that if the region is not on the top of the stack returns false
    assert(!mngo_regions__check_top(&region_manager,
        mngo_regions__find(&region_manager, region_names[0])));
    // Check that if the region is not on the stack returns false
    assert(!mngo_regions__check_top(&region_manager,
        mngo_regions__find(&region_manager, region_names[3])));

    mngo_regions__manager_finalize(&region_manager);
}
