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


#include <linux/limits.h>

#define SZ_PATH            PATH_MAX
#define SZ_PATH_SHORT      1024
#define SZ_PATH_INCOMPLETE SZ_PATH - (SZ_PATH_SHORT * 2)
#define SZ_NAME_SHORT      128

#include "common/types/generic.h"
#include "common/types/configuration/node_conf.h"

typedef struct dummy {
    int dummy;
} dummy_t;

typedef dummy_t eard_conf_t;
typedef dummy_t eargm_conf_t;
typedef dummy_t node_conf_t;
typedef dummy_t db_conf_t;
typedef dummy_t eardb_conf_t;
typedef dummy_t tag_t;
typedef dummy_t energy_tag_t;
typedef dummy_t node_island_t;
typedef dummy_t earlib_conf_t;
typedef dummy_t edcmon_t;


typedef struct conf_install {
    char dir_temp[SZ_PATH_INCOMPLETE];
    char dir_conf[SZ_PATH_INCOMPLETE];
    char dir_inst[SZ_PATH_INCOMPLETE];
    char dir_plug[SZ_PATH_INCOMPLETE];
    char obj_ener[SZ_PATH_SHORT];
    char obj_power_model[SZ_PATH_SHORT];
} conf_install_t;

/** Stores information about EAR installation. */
typedef struct cluster_conf {
    /**@{ Library & common configuration. */
    char DB_pathname[GENERIC_NAME];
    char net_ext[ID_SIZE];
    uint verbose;
    uint cluster_num_nodes;
    eard_conf_t eard;
    eargm_conf_t eargm;
    /**@}*/
    /**@{ List of policies. */
    uint num_policies;
    policy_conf_t *power_policies;
    uint default_policy;
    /**@}*/
    /**@{ List of authorized/admin users. */
    uint auth_users_count;
    char **auth_users;
    uint auth_groups_count;
    char **auth_groups;
    uint auth_acc_count;
    char **auth_accounts;
    uint admin_users_count;
    char **admin_users;
    /**@{ EAR owner of folders and files for
     * no specific users */
    char ear_owner[GENERIC_NAME];
    /**@}*/
    /**@{ Special cases. */
    uint min_time_perf_acc;
    /**@}*/
    /**@{ List of nodes. */
    uint num_nodes;
    node_conf_t *nodes;
    /**@}*/
    /**@{ Installation (other). */
    db_conf_t database;
    eardb_conf_t db_manager;
    uint num_tags;
    tag_t *tags;
    uint num_etags;
    energy_tag_t *e_tags;
    uint num_islands;
    node_island_t *islands;
    earlib_conf_t earlib;
    conf_install_t install;
    uint num_edcmons_tags;
    edcmon_t *edcmon_tags;
    /**@}*/
} cluster_conf_t;


int get_ear_conf_path(char *ear_conf_path);

my_node_conf_t *get_my_node_conf(cluster_conf_t *my_conf, char *nodename);

int read_cluster_conf(char *conf_path, cluster_conf_t *my_conf);
void free_cluster_conf(cluster_conf_t *conf);
