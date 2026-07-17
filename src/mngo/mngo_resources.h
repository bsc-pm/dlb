/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#ifndef DLB_MNGO_RESOURCES_H
#define DLB_MNGO_RESOURCES_H

#include "LB_core/spd.h"
#include <sched.h>
#include <stdbool.h>

typedef struct mngo_state_t {
    bool lewi_on;         // Tells if LeWI is beeing used
    cpu_set_t drom_mask;  // Tells the mask set by DROM
} mngo_state_t;

int mngo_state__load(subprocess_descriptor_t *spd, mngo_state_t *state);

#endif // DLB_MNGO_RESOURCES_H
