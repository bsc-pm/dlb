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

// This file contains portions derived from EAR library.
// Original copyright:
/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef NMETICS_ENERGY_CPU_H
#define NMETICS_ENERGY_CPU_H

#include "common/hardware/topology.h"
#include "common/states.h"
#include "common/types/generic.h"
#include "metrics/common/apis.h"

state_t energy_cpu_load(topology_t *tp, uint eard);

state_t energy_cpu_init(ctx_t *c);

state_t energy_cpu_dispose(ctx_t *c);

state_t energy_cpu_data_alloc(ctx_t *c, ullong **values, uint *rapl_count);

state_t energy_cpu_data_free(ctx_t *c, ullong **temp);

state_t energy_cpu_count_devices(ctx_t *c, uint *count);

state_t energy_cpu_read(ctx_t *c, ullong *values);

#endif
