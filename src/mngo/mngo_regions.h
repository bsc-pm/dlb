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

#ifndef MNGO_REGIONS_H
#define MNGO_REGIONS_H

#include "mngo/mngo_resources.h"
#include "mngo/mngo_talp.h"
#include "mngo/mngo_metrics.h"
#include "support/queues.h"

typedef struct mngo_region_info_t {
    mngo_state_t state;

    // The TALP monitor necessary for the metric gathering
    mngo_talp_region_t talp;

    // The history of metrics
    mngo_metrics_history_t metrics_history;
    // queue_t *metrics_history;
} mngo_region_info_t;

typedef struct dlb_mngo_region_t {
    const char * name;
    mngo_region_info_t region_info;
} dlb_mngo_region_t;

typedef struct {
    queue_t *regions;        // queue_t<dlb_mngo_region_t*>
    queue_t *active_regions; // queue_t<dlb_mngo_region_t*>
} mngo_regions_manager_t;

void mngo_regions__manager_init(mngo_regions_manager_t *region_manager, mngo_state_t *default_state);
void mngo_regions__manager_finalize(mngo_regions_manager_t *region_manager);

mngo_state_t * mngo_regions__get_current_resources(mngo_regions_manager_t *region_handler);

bool mngo_regions__check_top(const mngo_regions_manager_t *manager, const dlb_mngo_region_t *test);

int mngo_regions__push_active(mngo_regions_manager_t *manager, const dlb_mngo_region_t *new);
int mngo_regions__pop_active(mngo_regions_manager_t *manager, const dlb_mngo_region_t *old);

dlb_mngo_region_t * mngo_regions__find(const mngo_regions_manager_t *region_manager, const char *name);
dlb_mngo_region_t * mngo_regions__alloc(mngo_regions_manager_t*region_manager);

#endif /* MNGO_REGIONS_H */
