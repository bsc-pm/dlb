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

#ifndef TALP_HWC_H
#define TALP_HWC_H

#include "talp/talp_types.h"
#include <stdbool.h>

typedef struct SubProcessDescriptor subprocess_descriptor_t;
typedef struct hwc_measurements hwc_measurements_t;

int  talp_hwc_init(const subprocess_descriptor_t *spd);
int  talp_hwc_thread_init(void);
void talp_hwc_finalize(void);
void talp_hwc_thread_finalize(void);
void talp_hwc_on_state_change(talp_sample_state_t old_state, talp_sample_state_t new_state);
void talp_hwc_submit(const hwc_measurements_t *raw);
bool talp_hwc_collect(hwc_measurements_t *out);

#endif /* TALP_HWC_H */
