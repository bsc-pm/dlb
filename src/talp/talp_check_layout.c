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

#include "apis/dlb_talp.h"
#include <stddef.h>
#include <stdint.h>


/*** dlb_monitor_t ***/

size_t sizeof_dlb_monitor_t(void) {
    return sizeof(dlb_monitor_t);
}

#define DLB_MONITOR_FIELDS          \
    EXPAND(name)                    \
    EXPAND(num_cpus)                \
    EXPAND(avg_cpus)                \
    EXPAND(cycles)                  \
    EXPAND(instructions)            \
    EXPAND(num_measurements)        \
    EXPAND(num_resets)              \
    EXPAND(num_mpi_calls)           \
    EXPAND(num_omp_parallels)       \
    EXPAND(num_omp_tasks)           \
    EXPAND(num_gpu_runtime_calls)   \
    EXPAND(start_time)              \
    EXPAND(stop_time)               \
    EXPAND(elapsed_time)            \
    EXPAND(useful_time)             \
    EXPAND(mpi_time)                \
    EXPAND(omp_load_imbalance_time) \
    EXPAND(omp_scheduling_time)     \
    EXPAND(omp_serialization_time)  \
    EXPAND(gpu_runtime_time)        \
    EXPAND(gpu_useful_time)         \
    EXPAND(gpu_communication_time)  \
    EXPAND(gpu_inactive_time)       \
    EXPAND(_data)

#define EXPAND(field)                           \
size_t offset_dlb_monitor_t_##field(void) {     \
    return offsetof(dlb_monitor_t, field);      \
}
DLB_MONITOR_FIELDS
#undef EXPAND


/*** dlb_pop_metrics_t ***/

size_t sizeof_dlb_pop_metrics_t(void) {
    return sizeof(dlb_pop_metrics_t);
}

#define DLB_POP_METRICS_FIELDS           \
    EXPAND(name)                         \
    EXPAND(num_cpus)                     \
    EXPAND(num_mpi_ranks)                \
    EXPAND(num_nodes)                    \
    EXPAND(avg_cpus)                     \
    EXPAND(num_gpus)                     \
    EXPAND(cycles)                       \
    EXPAND(instructions)                 \
    EXPAND(num_measurements)             \
    EXPAND(num_mpi_calls)                \
    EXPAND(num_omp_parallels)            \
    EXPAND(num_omp_tasks)                \
    EXPAND(num_gpu_runtime_calls)        \
    EXPAND(elapsed_time)                 \
    EXPAND(useful_time)                  \
    EXPAND(mpi_time)                     \
    EXPAND(omp_load_imbalance_time)      \
    EXPAND(omp_scheduling_time)          \
    EXPAND(omp_serialization_time)       \
    EXPAND(gpu_runtime_time)             \
    EXPAND(min_mpi_normd_proc)           \
    EXPAND(min_mpi_normd_node)           \
    EXPAND(gpu_useful_time)              \
    EXPAND(gpu_communication_time)       \
    EXPAND(gpu_inactive_time)            \
    EXPAND(max_gpu_useful_time)          \
    EXPAND(max_gpu_active_time)          \
    EXPAND(parallel_efficiency)          \
    EXPAND(mpi_parallel_efficiency)      \
    EXPAND(mpi_communication_efficiency) \
    EXPAND(mpi_load_balance)             \
    EXPAND(mpi_load_balance_in)          \
    EXPAND(mpi_load_balance_out)         \
    EXPAND(omp_parallel_efficiency)      \
    EXPAND(omp_load_balance)             \
    EXPAND(omp_scheduling_efficiency)    \
    EXPAND(omp_serialization_efficiency) \
    EXPAND(device_offload_efficiency)    \
    EXPAND(gpu_parallel_efficiency)      \
    EXPAND(gpu_load_balance)             \
    EXPAND(gpu_communication_efficiency) \
    EXPAND(gpu_orchestration_efficiency)


#define EXPAND(field)                               \
size_t offset_dlb_pop_metrics_t_##field(void) {     \
    return offsetof(dlb_pop_metrics_t, field);      \
}
DLB_POP_METRICS_FIELDS
#undef EXPAND


/*** dlb_node_metrics_t ***/

size_t sizeof_dlb_node_metrics_t(void) {
    return sizeof(dlb_node_metrics_t);
}

#define DLB_NODE_METRICS_FIELDS      \
    EXPAND(name)                     \
    EXPAND(node_id)                  \
    EXPAND(processes_per_node)       \
    EXPAND(total_useful_time)        \
    EXPAND(total_mpi_time)           \
    EXPAND(max_useful_time)          \
    EXPAND(max_mpi_time)             \
    EXPAND(parallel_efficiency)      \
    EXPAND(communication_efficiency) \
    EXPAND(load_balance)

#define EXPAND(field)                               \
size_t offset_dlb_node_metrics_t_##field(void) {     \
    return offsetof(dlb_node_metrics_t, field);      \
}
DLB_NODE_METRICS_FIELDS
#undef EXPAND
