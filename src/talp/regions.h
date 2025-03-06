/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef REGIONS_H
#define REGIONS_H

#include <stdbool.h>

typedef struct dlb_monitor_t dlb_monitor_t;
typedef struct SubProcessDescriptor subprocess_descriptor_t;

/* Global region getters */
struct dlb_monitor_t* region_get_global(const subprocess_descriptor_t *spd);
const char* region_get_global_name(void);

/* Helper functions for GTree structures */
int  region_compare_by_name(const void *a, const void *b);
void region_dealloc(void *data);

/* Region functions */
dlb_monitor_t*
     region_register(const subprocess_descriptor_t *spd, const char* name);
int  region_reset(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor);
int  region_start(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor);
int  region_stop(const subprocess_descriptor_t *spd, dlb_monitor_t *monitor);
bool region_is_started(const dlb_monitor_t *monitor);
void region_set_internal(struct dlb_monitor_t *monitor, bool internal);
int  region_report(const subprocess_descriptor_t *spd, const dlb_monitor_t *monitor);


#endif /* REGIONS_H */
