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

#include "mngo/mngo_regions.h"
#include "dlb_errors.h"
#include "mngo/mngo_resources.h"
#include "support/debug.h"
#include <stdio.h>
#include <string.h>

static mngo_state_t *_default_state;

void mngo_regions__manager_init(mngo_regions_manager_t *region_manager,
                                mngo_state_t *default_state) {
    region_manager->regions =
        queue__init(sizeof(dlb_mngo_region_t *), 1, NULL, QUEUE_ALLOC_REALLOC);

    region_manager->active_regions =
        queue__init(sizeof(dlb_mngo_region_t *), 1, NULL, QUEUE_ALLOC_REALLOC);

    _default_state = default_state;
}

static void free_element(void *args, void *curr) {
    dlb_mngo_region_t **region = curr;
    free(*region);
}

void mngo_regions__manager_finalize(mngo_regions_manager_t *region_manager) {
    queue_iter_head2tail_t iter =
        queue__into_head2tail_iter(region_manager->regions);
    queue_iter__foreach(&iter, free_element, NULL);

    queue__destroy(region_manager->regions);
    region_manager->regions = NULL;
    queue__destroy(region_manager->active_regions);
    region_manager->active_regions = NULL;
}

mngo_state_t *
mngo_regions__get_current_resources(mngo_regions_manager_t *manager) {
    dlb_mngo_region_t **region;
    if (queue__peek_head(manager->active_regions, (void **)&region) ==
        DLB_SUCCESS) {
        return &(*region)->region_info.state;
    } else {
        return _default_state;
    }
}

bool mngo_regions__check_top(const mngo_regions_manager_t *manager,
                             const dlb_mngo_region_t *region) {
    dlb_mngo_region_t **current_region;
    if (queue__peek_head(manager->active_regions, (void **)&current_region) !=
        DLB_SUCCESS) {
        return false;
    }

    if (strcmp((*current_region)->name, region->name) != 0) {
        return false;
    }

    return true;
}

int mngo_regions__push_active(mngo_regions_manager_t *manager,
                              const dlb_mngo_region_t *new_region) {
    if (queue__push_head(manager->active_regions, &new_region) == NULL) {
        verbose(VB_MNGO, "Could not push new region (%s) into active queue.",
                new_region->name);
    }

    return DLB_SUCCESS;
}

int mngo_regions__pop_active(mngo_regions_manager_t *manager,
                             const dlb_mngo_region_t *old_region) {
    if (!mngo_regions__check_top(manager, old_region)) {
        verbose(VB_MNGO, "region (%s) is not on top", old_region->name);
        return DLB_NOUPDT;
    }

    queue__take_head(manager->active_regions, NULL);

    return DLB_SUCCESS;
}

struct mngo_regions_find_foreach_t {
    const char *name;
    dlb_mngo_region_t *found;
};

static void mngo_regions_find_foreach(void *args, void *curr) {
    struct mngo_regions_find_foreach_t *a = args;
    dlb_mngo_region_t **region = curr;

    if (strcmp((*region)->name, a->name) == 0) {
        a->found = *region;
    }
}

dlb_mngo_region_t *
mngo_regions__find(const mngo_regions_manager_t *region_manager,
                   const char *name) {
    queue_iter_head2tail_t iter =
        queue__into_head2tail_iter(region_manager->regions);

    struct mngo_regions_find_foreach_t find_cfg = {
        .name = name,
        .found = NULL,
    };

    queue_iter__foreach(&iter, mngo_regions_find_foreach, &find_cfg);

    return find_cfg.found;
}

dlb_mngo_region_t *mngo_regions__alloc(mngo_regions_manager_t *region_manager) {
    dlb_mngo_region_t **new_region_place =
        queue__emplace_head(region_manager->regions);
    *new_region_place = malloc(sizeof(dlb_mngo_region_t));
    return *new_region_place;
}
