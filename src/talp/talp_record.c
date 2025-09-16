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

#include "talp/talp_record.h"

#include "LB_comm/shmem_talp.h"
#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "talp/perf_metrics.h"
#include "talp/regions.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"
#ifdef MPI_LIB
#include "mpi/mpi_core.h"
#endif

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>


/*********************************************************************************/
/*    TALP Record in serial (non-MPI) mode                                       */
/*********************************************************************************/

/* For any given monitor, record metrics considering only this (sub-)process */
void talp_record_monitor(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {
    if (spd->options.talp_summary & SUMMARY_PROCESS) {
        verbose(VB_TALP, "TALP process summary: recording region %s", monitor->name);

        process_record_t process_record = {
            .rank = 0,
            .pid = spd->id,
            .monitor = *monitor,
        };

        /* Fill hostname and CPU mask strings in process_record */
        gethostname(process_record.hostname, HOST_NAME_MAX);
        snprintf(process_record.cpuset, TALP_OUTPUT_CPUSET_MAX, "%s",
                mu_to_str(&spd->process_mask));
        mu_get_quoted_mask(&spd->process_mask, process_record.cpuset_quoted,
            TALP_OUTPUT_CPUSET_MAX);

        /* Add record */
        talp_output_record_process(monitor->name, &process_record, 1);
    }

    if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
        if (monitor->elapsed_time > 0) {
            verbose(VB_TALP, "TALP summary: recording region %s", monitor->name);

            bool have_gpus = (monitor->gpu_useful_time + monitor->gpu_communication_time > 0);

            talp_info_t *talp_info = spd->talp_info;
            const pop_base_metrics_t base_metrics = {
                .num_cpus                = monitor->num_cpus,
                .num_mpi_ranks           = 0,
                .num_nodes               = 1,
                .avg_cpus                = monitor->avg_cpus,
                .num_gpus                = have_gpus ? 1 : 0,
                .cycles                  = (double)monitor->cycles,
                .instructions            = (double)monitor->instructions,
                .num_measurements        = monitor->num_measurements,
                .num_mpi_calls           = monitor->num_mpi_calls,
                .num_omp_parallels       = monitor->num_omp_parallels,
                .num_omp_tasks           = monitor->num_omp_tasks,
                .num_gpu_runtime_calls   = monitor->num_gpu_runtime_calls,
                .elapsed_time            = monitor->elapsed_time,
                .useful_time             = monitor->useful_time,
                .mpi_time                = monitor->mpi_time,
                .omp_load_imbalance_time = monitor->omp_load_imbalance_time,
                .omp_scheduling_time     = monitor->omp_scheduling_time,
                .omp_serialization_time  = monitor->omp_serialization_time,
                .gpu_runtime_time        = monitor->gpu_runtime_time,
                .min_mpi_normd_proc      = (double)monitor->mpi_time / monitor->num_cpus,
                .min_mpi_normd_node      = (double)monitor->mpi_time / monitor->num_cpus,
                .gpu_useful_time         = monitor->gpu_useful_time,
                .gpu_communication_time  = monitor->gpu_communication_time,
                .gpu_inactive_time       = monitor->gpu_inactive_time,
                .max_gpu_useful_time     = monitor->gpu_useful_time,
                .max_gpu_active_time     = monitor->gpu_useful_time + monitor->gpu_communication_time,
            };

            dlb_pop_metrics_t pop_metrics;
            perf_metrics__base_to_pop_metrics(monitor->name, &base_metrics, &pop_metrics);
            talp_output_record_pop_metrics(&pop_metrics);

            if(monitor == talp_info->monitor) {
                talp_output_record_resources(monitor->num_cpus,
                        /* num_nodes */ 1, /* num_ranks */ 0);
            }

        } else {
            verbose(VB_TALP, "TALP summary: recording empty region %s", monitor->name);
            dlb_pop_metrics_t pop_metrics = {0};
            snprintf(pop_metrics.name, DLB_MONITOR_NAME_MAX, "%s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);
        }
    }
}


/*********************************************************************************/
/*    TALP Record in MPI mode                                                    */
/*********************************************************************************/

#if MPI_LIB

/* Compute Node summary of all Global Monitors and record data */
void talp_record_node_summary(const subprocess_descriptor_t *spd) {
    node_record_t *node_summary = NULL;
    size_t node_summary_size = 0;

    /* Perform a barrier so that all processes in the node have arrived at the
     * MPI_Finalize */
    node_barrier(spd, NULL);

    /* Node process 0 reduces all global regions from all processes in the node */
    if (_process_id == 0) {
        /* Obtain a list of regions associated with the Global Region Name, sorted by PID */
        int max_procs = mu_get_system_size();
        talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
        int nelems;
        shmem_talp__get_regionlist(region_list, &nelems, max_procs, region_get_global_name());

        /* Allocate and initialize node summary structure */
        node_summary_size = sizeof(node_record_t) + sizeof(process_in_node_record_t) * nelems;
        node_summary = malloc(node_summary_size);
        *node_summary = (const node_record_t) {
            .node_id = _node_id,
            .nelems = nelems,
        };

        /* Iterate the PID list and gather times of every process */
        for (int i = 0; i < nelems; ++i) {
            int64_t mpi_time = region_list[i].mpi_time;
            int64_t useful_time = region_list[i].useful_time;

            /* Save times in local structure */
            node_summary->processes[i].pid = region_list[i].pid;
            node_summary->processes[i].mpi_time = mpi_time;
            node_summary->processes[i].useful_time = useful_time;

            /* Accumulate total and max values */
            node_summary->avg_useful_time += useful_time;
            node_summary->avg_mpi_time += mpi_time;
            node_summary->max_useful_time = max_int64(useful_time, node_summary->max_useful_time);
            node_summary->max_mpi_time = max_int64(mpi_time, node_summary->max_mpi_time);
        }
        free(region_list);

        /* Compute average values */
        node_summary->avg_useful_time /= node_summary->nelems;
        node_summary->avg_mpi_time /= node_summary->nelems;
    }

    /* Perform a final barrier so that all processes let the _process_id 0 to
     * gather all the data */
    node_barrier(spd, NULL);

    /* All main processes from each node send data to rank 0 */
    if (_process_id == 0) {
        verbose(VB_TALP, "Node summary: gathering data");

        /* MPI type: int64_t */
        MPI_Datatype mpi_int64_type = get_mpi_int64_type();

        /* MPI type: pid_t */
        MPI_Datatype mpi_pid_type;
        PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(pid_t), &mpi_pid_type);

        /* MPI struct type: process_in_node_record_t */
        MPI_Datatype mpi_process_info_type;
        {
            int count = 3;
            int blocklengths[] = {1, 1, 1};
            MPI_Aint displacements[] = {
                offsetof(process_in_node_record_t, pid),
                offsetof(process_in_node_record_t, mpi_time),
                offsetof(process_in_node_record_t, useful_time)};
            MPI_Datatype types[] = {mpi_pid_type, mpi_int64_type, mpi_int64_type};
            MPI_Datatype tmp_type;
            PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
            PMPI_Type_create_resized(tmp_type, 0, sizeof(process_in_node_record_t),
                    &mpi_process_info_type);
            PMPI_Type_commit(&mpi_process_info_type);
        }

        /* MPI struct type: node_record_t */
        MPI_Datatype mpi_node_record_type;;
        {
            int count = 7;
            int blocklengths[] = {1, 1, 1, 1, 1, 1, node_summary->nelems};
            MPI_Aint displacements[] = {
                offsetof(node_record_t, node_id),
                offsetof(node_record_t, nelems),
                offsetof(node_record_t, avg_useful_time),
                offsetof(node_record_t, avg_mpi_time),
                offsetof(node_record_t, max_useful_time),
                offsetof(node_record_t, max_mpi_time),
                offsetof(node_record_t, processes)};
            MPI_Datatype types[] = {MPI_INT, MPI_INT, mpi_int64_type, mpi_int64_type,
                mpi_int64_type, mpi_int64_type, mpi_process_info_type};
            MPI_Datatype tmp_type;
            PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
            PMPI_Type_create_resized(tmp_type, 0, node_summary_size, &mpi_node_record_type);
            PMPI_Type_commit(&mpi_node_record_type);
        }

        /* Gather data */
        void *recvbuf = NULL;
        if (_mpi_rank == 0) {
            recvbuf = malloc(_num_nodes * node_summary_size);
        }
        PMPI_Gather(node_summary, 1, mpi_node_record_type,
                recvbuf, 1, mpi_node_record_type,
                0, getInterNodeComm());

        /* Free send buffer and MPI Datatypes */
        free(node_summary);
        PMPI_Type_free(&mpi_process_info_type);
        PMPI_Type_free(&mpi_node_record_type);

        /* Add records */
        if (_mpi_rank == 0) {
            for (int node_id = 0; node_id < _num_nodes; ++node_id) {
                verbose(VB_TALP, "Node summary: recording node %d", node_id);
                node_record_t *node_record = (node_record_t*)(
                        (unsigned char *)recvbuf + node_summary_size * node_id);
                ensure( node_id == node_record->node_id, "Node id error in %s", __func__ );
                talp_output_record_node(node_record);
            }
            free(recvbuf);
        }
    }
}

/* Gather PROCESS data of a monitor among all ranks and record it in rank 0 */
void talp_record_process_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {

    /* Internal monitors will not be recorded */
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return;
    }

    if (_mpi_rank == 0) {
        verbose(VB_TALP, "Process summary: gathering region %s", monitor->name);
    }

    process_record_t process_record_send = {
        .rank = _mpi_rank,
        .pid = spd->id,
        .node_id = _node_id,
        .monitor = *monitor,
    };

    /* Invalidate pointers of the copied monitor */
    process_record_send.monitor.name = NULL;
    process_record_send.monitor._data = NULL;

    /* Fill hostname and CPU mask strings in process_record_send */
    gethostname(process_record_send.hostname, HOST_NAME_MAX);
    snprintf(process_record_send.cpuset, TALP_OUTPUT_CPUSET_MAX, "%s",
            mu_to_str(&spd->process_mask));
    mu_get_quoted_mask(&spd->process_mask, process_record_send.cpuset_quoted,
            TALP_OUTPUT_CPUSET_MAX);

    /* MPI type: int64_t */
    MPI_Datatype mpi_int64_type = get_mpi_int64_type();

    /* MPI type: pid_t */
    MPI_Datatype mpi_pid_type;
    PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(pid_t), &mpi_pid_type);

    /* Note: obviously, it doesn't make sense to send addresses via MPI, but we
     * are sending the whole dlb_monitor_t, so... Addresses are discarded
     * either way. */

    /* MPI type: void* */
    MPI_Datatype address_type;
    PMPI_Type_match_size(MPI_TYPECLASS_INTEGER, sizeof(void*), &address_type);

    /* MPI struct type: dlb_monitor_t */
    MPI_Datatype mpi_dlb_monitor_type;
    {
        int blocklengths[] = {
            1, 1, 1,                /* Name + Resources: num_cpus, avg_cpus */
            1, 1,                   /* Hardware counters: cycles, instructions */
            1, 1, 1, 1, 1, 1,       /* Statistics: num_* */
            1, 1,                   /* Monitor Start and Stop times */
            1, 1, 1, 1, 1, 1, 1,    /* Host Times */
            1, 1, 1,                /* Device Times */
            1};                     /* _data */

        enum {count = sizeof(blocklengths) / sizeof(blocklengths[0])};

        MPI_Aint displacements[] = {
            offsetof(dlb_monitor_t, name),
            /* Resources */
            offsetof(dlb_monitor_t, num_cpus),
            offsetof(dlb_monitor_t, avg_cpus),
            /* Hardware counters */
            offsetof(dlb_monitor_t, cycles),
            offsetof(dlb_monitor_t, instructions),
            /* Statistics */
            offsetof(dlb_monitor_t, num_measurements),
            offsetof(dlb_monitor_t, num_resets),
            offsetof(dlb_monitor_t, num_mpi_calls),
            offsetof(dlb_monitor_t, num_omp_parallels),
            offsetof(dlb_monitor_t, num_omp_tasks),
            offsetof(dlb_monitor_t, num_gpu_runtime_calls),
            /* Monitor Start and Stop times */
            offsetof(dlb_monitor_t, start_time),
            offsetof(dlb_monitor_t, stop_time),
            /* Host Times */
            offsetof(dlb_monitor_t, elapsed_time),
            offsetof(dlb_monitor_t, useful_time),
            offsetof(dlb_monitor_t, mpi_time),
            offsetof(dlb_monitor_t, omp_load_imbalance_time),
            offsetof(dlb_monitor_t, omp_scheduling_time),
            offsetof(dlb_monitor_t, omp_serialization_time),
            offsetof(dlb_monitor_t, gpu_runtime_time),
            /* Device Times */
            offsetof(dlb_monitor_t, gpu_useful_time),
            offsetof(dlb_monitor_t, gpu_communication_time),
            offsetof(dlb_monitor_t, gpu_inactive_time),
            /* _data */
            offsetof(dlb_monitor_t, _data)};

        MPI_Datatype types[] = {
            address_type, MPI_INT, MPI_FLOAT,   /* Name + Resources: num_cpus, avg_cpus */
            mpi_int64_type, mpi_int64_type,     /* Hardware counters: cycles, instructions */
            MPI_INT, MPI_INT,
            mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type,     /* Statistics: num_* */
            mpi_int64_type, mpi_int64_type,     /* Monitor Start and Stop times */
            mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type,
            mpi_int64_type, mpi_int64_type,
            mpi_int64_type,                     /* Host Times */
            mpi_int64_type, mpi_int64_type,
            mpi_int64_type,                     /* Device Times */
            address_type};                      /* _data */

        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(dlb_monitor_t), &mpi_dlb_monitor_type);
        PMPI_Type_commit(&mpi_dlb_monitor_type);

        static_ensure(sizeof(blocklengths)/sizeof(blocklengths[0]) == count,
                "blocklengths size mismatch");
        static_ensure(sizeof(displacements)/sizeof(displacements[0]) == count,
                "displacements size mismatch");
        static_ensure(sizeof(types)/sizeof(types[0]) == count,
                "types size mismatch");
    }

    /* MPI struct type: process_record_t */
    MPI_Datatype mpi_process_record_type;
    {
        int count = 7;
        int blocklengths[] = {1, 1, 1, HOST_NAME_MAX,
            TALP_OUTPUT_CPUSET_MAX, TALP_OUTPUT_CPUSET_MAX, 1};
        MPI_Aint displacements[] = {
            offsetof(process_record_t, rank),
            offsetof(process_record_t, pid),
            offsetof(process_record_t, node_id),
            offsetof(process_record_t, hostname),
            offsetof(process_record_t, cpuset),
            offsetof(process_record_t, cpuset_quoted),
            offsetof(process_record_t, monitor)};
        MPI_Datatype types[] = {MPI_INT, mpi_pid_type, MPI_INT, MPI_CHAR, MPI_CHAR,
            MPI_CHAR, mpi_dlb_monitor_type};
        MPI_Datatype tmp_type;
        PMPI_Type_create_struct(count, blocklengths, displacements, types, &tmp_type);
        PMPI_Type_create_resized(tmp_type, 0, sizeof(process_record_t),
                &mpi_process_record_type);
        PMPI_Type_commit(&mpi_process_record_type);
    }

    /* Gather data */
    process_record_t *recvbuf = NULL;
    if (_mpi_rank == 0) {
        recvbuf = malloc(_mpi_size * sizeof(process_record_t));
    }
    PMPI_Gather(&process_record_send, 1, mpi_process_record_type,
            recvbuf, 1, mpi_process_record_type,
            0, getWorldComm());

    /* Add records */
    if (_mpi_rank == 0) {
        for (int rank = 0; rank < _mpi_size; ++rank) {
            verbose(VB_TALP, "Process summary: recording region %s on rank %d",
                    monitor->name, rank);
            talp_output_record_process(monitor->name, &recvbuf[rank], _mpi_size);
        }
        free(recvbuf);
    }

    /* Free MPI types */
    PMPI_Type_free(&mpi_dlb_monitor_type);
    PMPI_Type_free(&mpi_process_record_type);
}

/* Gather POP METRICS data of a monitor among all ranks and record it in rank 0 */
void talp_record_pop_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor) {

    /* Internal monitors will not be recorded */
    if (((monitor_data_t*)monitor->_data)->flags.internal) {
        return;
    }

    if (_mpi_rank == 0) {
        verbose(VB_TALP, "TALP summary: gathering region %s", monitor->name);
    }

    talp_info_t *talp_info = spd->talp_info;

    /* Reduce monitor among all MPI ranks into MPI rank 0 */
    pop_base_metrics_t base_metrics;
    perf_metrics__reduce_monitor_into_base_metrics(&base_metrics, monitor, false);

    if (_mpi_rank == 0) {
        if (base_metrics.elapsed_time > 0) {

            /* Only the global region records the resources */
            if (monitor == talp_info->monitor) {
                talp_output_record_resources(base_metrics.num_cpus,
                        base_metrics.num_nodes, base_metrics.num_mpi_ranks);
            }

            /* Construct pop_metrics out of base metrics */
            dlb_pop_metrics_t pop_metrics;
            perf_metrics__base_to_pop_metrics(monitor->name, &base_metrics, &pop_metrics);

            /* Record */
            verbose(VB_TALP, "TALP summary: recording region %s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);

        } else {
            /* Record empty */
            verbose(VB_TALP, "TALP summary: recording empty region %s", monitor->name);
            dlb_pop_metrics_t pop_metrics = {0};
            snprintf(pop_metrics.name, DLB_MONITOR_NAME_MAX, "%s", monitor->name);
            talp_output_record_pop_metrics(&pop_metrics);
        }
    }
}

#endif /* MPI_LIB */
