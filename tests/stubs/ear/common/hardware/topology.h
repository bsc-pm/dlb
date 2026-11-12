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

#ifndef COMMON_HARDWARE_TOPOLOGY_H_
#define COMMON_HARDWARE_TOPOLOGY_H_

#include "common/states.h"

#define TOPO_CL_COUNT  (4 + 1) // Cache levels count, L0 to L4, but 0 doesn't exists

typedef struct cl_s {
    int id;
} cl_t;

typedef struct cpu_s {
    int id;                //
    int apicid;            // Taken from
    int is_thread;         // If is the secondary thread in a core.
    int sibling_id;        // The thread sibling within a core.
    int core_id;           // Basically the minor sibling id
    int socket_id;         // Socket id taken from 'physical_package_id' file
    int socket_idx;        // Socket index from 0 to socket_count, best option to work with arrays
    cl_t l[TOPO_CL_COUNT]; // Index = level, 0 doesn't exists
} cpu_t;

typedef struct topology_s {
    cpu_t *cpus;          // Take a look to cpu_t structure.
    int cpu_count;        // Total CPUs including threads.
    int core_count;       // Total cores (not counting threads).
    int socket_count;     //
    int threads_per_core; // Number or threads per core (not the whole system).
    int smt_enabled;      // Multithreading enabled = 1, disabled = 0.
    int l2_count;         // Chunks of L2 in the system.
    int l3_count;         // Chunks of L3 in the system.
    int l4_count;         // Chunks of L4 in the system.
    int cache_last_level; // Returns the maximum cache level
    int cache_line_size;
    int vendor; // Take a look to top defines.
    int family; // Take a look to top defines.
    int model;  // Take a look to top defines.
    char brand[48];
    int gpr_count;     // Number of general purpose registers.
    int gpr_bits;      // General purpose registers bit width.
    int nmi_watchdog;  // NMI watdog enabled.
    int apicids_found; // List of CPUs ApicId's found.
    int initialized;   // 1 if the topology is initialized
    int avx512;        // 1 if AVX-512 is enabled
    int sve;           // 1 if ARM-SVE is enabled
    int sve_bits;      // Counts the ARM-SVE vector bits
    int tdp;
} topology_t;

state_t topology_init(topology_t *topo);

#endif
