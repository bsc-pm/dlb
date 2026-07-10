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

#ifndef TALP_H
#define TALP_H

#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "talp/talp_types.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct SubProcessDescriptor subprocess_descriptor_t;


/* TALP init / finalize */
void talp_init(subprocess_descriptor_t *spd);
void talp_finalize(subprocess_descriptor_t *spd);


/* TALP samples aggregation */
int talp_aggregate_samples_to_regions(talp_info_t *talp_info);


/* TALP collect functions for 3rd party programs */
int talp_query_pop_node_metrics(const char *name, struct dlb_node_metrics_t *node_metrics);


/* TALP collect functions for 1st party programs */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_pop_metrics_t *pop_metrics);
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_node_metrics_t *node_metrics);


#endif /* TALP_H */
