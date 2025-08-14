/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#ifndef DLB_CORE_TALP_H
#define DLB_CORE_TALP_H

#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "talp/talp_types.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>

enum { TALP_NO_TIMESTAMP = 0 };

typedef struct SubProcessDescriptor subprocess_descriptor_t;


/* PAPI counters */
int  talp_init_papi_counters(void);
void talp_fini_papi_counters(void);

/* TALP init / finalize */
void talp_init(subprocess_descriptor_t *spd);
void talp_finalize(subprocess_descriptor_t *spd);


/* TALP samples */
talp_sample_t*
     talp_get_thread_sample(const subprocess_descriptor_t *spd);
void talp_set_sample_state(talp_sample_t *sample, enum talp_sample_state state, bool papi);
void talp_update_sample(talp_sample_t *sample, bool papi, int64_t timestamp);
int  talp_flush_samples_to_regions(const subprocess_descriptor_t *spd);
void talp_flush_sample_subset_to_regions(const subprocess_descriptor_t *spd,
        talp_sample_t **samples, unsigned int nelems);


/* TALP collect functions for 3rd party programs */
int talp_query_pop_node_metrics(const char *name, struct dlb_node_metrics_t *node_metrics);


/* TALP collect functions for 1st party programs */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_pop_metrics_t *pop_metrics);
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        struct dlb_monitor_t *monitor, struct dlb_node_metrics_t *node_metrics);


#endif
