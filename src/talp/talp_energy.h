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

#ifndef TALP_ENERGY_H
#define TALP_ENERGY_H

#include <stdbool.h>
#include <stdint.h>

#include "talp/backend.h"

typedef struct SubProcessDescriptor subprocess_descriptor_t;

int  talp_energy_init(const subprocess_descriptor_t *spd);
void talp_energy_finalize(void);

/* Collects energy (nJ) and interval (ns) since the last call */
bool talp_energy_collect(energy_readings_t *out, int64_t *interval_ns);

#endif /* TALP_ENERGY_H */
