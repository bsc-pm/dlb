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

#ifndef METRICS_GPU_H
#define METRICS_GPU_H

#include "common/states.h"
#include "common/types/generic.h"
#include "metrics/common/apis.h"
#include <time.h>

typedef struct timespec timestamp_t;

typedef struct gpu_devs_s {
    ullong serial;
    uint index;
} gpu_devs_t;

typedef struct gpu_s {
    timestamp_t time;
    ulong samples;
    ulong freq_gpu; // khz
    ulong freq_mem; // khz
    ulong util_gpu; // percent
    ulong util_mem; // percent
    ulong temp_gpu; // celsius
    ulong temp_mem; // celsius
    double energy_j;
    double power_w;
    uint working;
    uint correct;
} gpu_t;

void gpu_load(int force_api);

void gpu_get_devices(gpu_devs_t **devs, uint *devs_count);

state_t gpu_init(ctx_t *c);

state_t gpu_dispose(ctx_t *c);

state_t gpu_read(ctx_t *c, gpu_t *data);

void gpu_data_alloc(gpu_t **data);

void gpu_data_free(gpu_t **data);

#endif
