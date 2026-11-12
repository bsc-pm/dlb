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

#include "common/hardware/topology.h"
#include "common/states.h"
#include "common/types/configuration/cluster_conf.h"
#include "common/types/generic.h"
#include "daemon/local_api/eard_api_conf.h"
#include "metrics/common/apis.h"
#include "metrics/energy_cpu/energy_cpu.h"
#include "metrics/gpu/gpu.h"

/* ----- EAR daemon ----- */

int eards_connection(void)
{
    return EAR_SUCCESS;
}

int eards_connected(void)
{
    return EAR_SUCCESS;
}

void eards_disconnect(void)
{
}

/* ----- Topology ----- */

state_t topology_init(topology_t *topo)
{
    return EAR_SUCCESS;
}

/* ----- Cluster conf ----- */

int get_ear_conf_path(char *ear_conf_path)
{
    return 0;
}

my_node_conf_t *get_my_node_conf(cluster_conf_t *my_conf, char *nodename)
{
    return NULL;
}

int read_cluster_conf(char *conf_path, cluster_conf_t *my_conf)
{
    return 0;
}

void free_cluster_conf(cluster_conf_t *conf)
{
}

/* ----- CPU ----- */

state_t energy_cpu_load(topology_t *tp, uint eard)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_init(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_data_alloc(ctx_t *c, ullong **values, uint *rapl_count)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_data_free(ctx_t *c, ullong **temp)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_count_devices(ctx_t *c, uint *count)
{
    return EAR_SUCCESS;
}

state_t energy_cpu_read(ctx_t *c, ullong *values)
{
    return EAR_SUCCESS;
}

/* ----- GPU ----- */

void gpu_load(int force_api)
{
}

void gpu_get_devices(gpu_devs_t **devs, uint *devs_count)
{
}

state_t gpu_init(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t gpu_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t gpu_read(ctx_t *c, gpu_t *data)
{
    return EAR_SUCCESS;
}

void gpu_data_alloc(gpu_t **data)
{
}

void gpu_data_free(gpu_t **data)
{
}
