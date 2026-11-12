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

#pragma once

#include "common/types/generic.h"

#define MAX_POLICY_SETTINGS    5
#define POLICY_NAME_SIZE       64

typedef struct policy_conf {
    uint policy;
    char name[POLICY_NAME_SIZE];
    char tag[POLICY_NAME_SIZE];
    double settings[MAX_POLICY_SETTINGS];
    float def_freq;
    uint p_state;
    uint is_available;
} policy_conf_t;

#define NODE_PREFIX       64
#define GENERIC_NODE_NAME 256

typedef struct my_node_conf {
    uint cpus;
    uint island;
    ulong max_pstate;
    char db_ip[NODE_PREFIX];
    char db_sec_ip[NODE_PREFIX];
    char *coef_file;
    char *energy_plugin;
    char *energy_model;
    char *idle_governor;
    char *powercap_plugin;
    char *powercap_node_plugin;
    char *powercap_gpu_plugin;
    uint num_policies;
    policy_conf_t *policies;
    double min_sig_power;
    double max_sig_power;
    double max_error_power;
    ulong max_temp;
    ulong max_avx512_freq;
    ulong max_avx2_freq;
    long max_powercap;
    long powercap;
    char powercap_type;
    ulong gpu_def_freq;
    int cpu_max_pstate;
    int imc_max_pstate;
    int idle_pstate;
    ulong imc_max_freq;
    ulong imc_min_freq;
    uint use_log;
    char tag[GENERIC_NAME];
} my_node_conf_t;
