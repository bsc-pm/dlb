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

#include "talp/talp_mpi.h"

#include "LB_comm/shmem_talp.h"
#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_record.h"
#include "talp/talp_types.h"
#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

#include <stdio.h>
#include <string.h>

extern __thread bool thread_is_observer;


#ifdef MPI_LIB
/* Communicate among all MPI processes so that everyone has the same monitoring regions */
static void talp_register_common_mpi_regions(const subprocess_descriptor_t *spd) {
    /* Note: there's a potential race condition if this function is called
     * (which happens on talp_mpi_finalize or talp_finalize) while another
     * thread creates a monitoring region. The solution would be to lock the
     * entire routine and call a specialized registering function that does not
     * lock, or use a recursive lock. The situation is strange enough to not
     * support it */

    talp_info_t *talp_info = spd->talp_info;

    /* Warn about open regions */
    for (GSList *node = talp_info->open_regions;
            node != NULL;
            node = node->next) {
        const dlb_monitor_t *monitor = node->data;
        warning("Region %s is still open during MPI_Finalize."
                " Collected data may be incomplete.",
                monitor->name);
    }

    /* Gather recvcounts for each process
        * (Each process may have different number of monitors) */
    int nregions = g_tree_nnodes(talp_info->regions);
    int chars_to_send = nregions * DLB_MONITOR_NAME_MAX;
    int *recvcounts = malloc(_mpi_size * sizeof(int));
    PMPI_Allgather(&chars_to_send, 1, MPI_INT,
            recvcounts, 1, MPI_INT, getWorldComm());

    /* Compute total characters to gather via MPI */
    int i;
    int total_chars = 0;
    for (i=0; i<_mpi_size; ++i) {
        total_chars += recvcounts[i];
    }

    if (total_chars > 0) {
        /* Prepare sendbuffer */
        char *sendbuffer = malloc(nregions * DLB_MONITOR_NAME_MAX * sizeof(char));
        char *sendptr = sendbuffer;
        for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                node != NULL;
                node = g_tree_node_next(node)) {
            const dlb_monitor_t *monitor = g_tree_node_value(node);
            strcpy(sendptr, monitor->name);
            sendptr += DLB_MONITOR_NAME_MAX;
        }

        /* Prepare recvbuffer */
        char *recvbuffer = malloc(total_chars * sizeof(char));

        /* Compute displacements */
        int *displs = malloc(_mpi_size * sizeof(int));
        int next_disp = 0;
        for (i=0; i<_mpi_size; ++i) {
            displs[i] = next_disp;
            next_disp += recvcounts[i];
        }

        /* Gather all regions */
        PMPI_Allgatherv(sendbuffer, nregions * DLB_MONITOR_NAME_MAX, MPI_CHAR,
                recvbuffer, recvcounts, displs, MPI_CHAR, getWorldComm());

        /* Register all regions. Existing ones will be skipped. */
        for (i=0; i<total_chars; i+=DLB_MONITOR_NAME_MAX) {
            region_register(spd, &recvbuffer[i]);
        }

        free(sendbuffer);
        free(recvbuffer);
        free(displs);
    }

    free(recvcounts);
}
#endif


/*********************************************************************************/
/*    TALP MPI functions                                                         */
/*********************************************************************************/

/* Start global monitoring region (if not already started) */
void talp_mpi_init(const subprocess_descriptor_t *spd) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_mpi_init");

    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        talp_info->flags.have_mpi = true;

        /* Start global region (no-op if already started) */
        region_start(spd, talp_info->monitor);

        /* Add MPI_Init statistic and set useful state */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

/* Stop global monitoring region and gather APP data if needed  */
void talp_mpi_finalize(const subprocess_descriptor_t *spd) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_mpi_finalize");
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Add MPI_Finalize */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);

        /* Stop global region */
        region_stop(spd, talp_info->monitor);

        monitor_data_t *monitor_data = talp_info->monitor->_data;

        /* Update shared memory values */
        if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
            // TODO: is it needed? isn't it updated when stopped?
            shmem_talp__set_times(monitor_data->node_shared_id,
                    talp_info->monitor->mpi_time,
                    talp_info->monitor->useful_time);
        }

#ifdef MPI_LIB
        /* If performing any kind of TALP summary, check that the number of processes
         * registered in the shared memory matches with the number of MPI processes in the node.
         * This check is needed to avoid deadlocks on finalize. */
        if (spd->options.talp_summary) {
            verbose(VB_TALP, "Gathering TALP metrics");
            /* FIXME: use Named Barrier */
            /* if (shmem_barrier__get_num_participants(spd->options.barrier_id) == _mpis_per_node) { */

                /* Gather data among processes in the node if node summary is enabled */
                if (spd->options.talp_summary & SUMMARY_NODE) {
                    talp_record_node_summary(spd);
                }

                /* Gather data among MPIs if any of these summaries is enabled  */
                if (spd->options.talp_summary
                        & (SUMMARY_POP_METRICS | SUMMARY_PROCESS)) {
                    /* Ensure everyone has the same monitoring regions */
                    talp_register_common_mpi_regions(spd);

                    /* Finally, reduce data */
                    for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                            node != NULL;
                            node = g_tree_node_next(node)) {
                        const dlb_monitor_t *monitor = g_tree_node_value(node);
                        if (spd->options.talp_summary & SUMMARY_POP_METRICS) {
                            talp_record_pop_summary(spd, monitor);
                        }
                        if (spd->options.talp_summary & SUMMARY_PROCESS) {
                            talp_record_process_summary(spd, monitor);
                        }
                    }
                }

                /* Synchronize all processes in node before continuing with DLB finalization  */
                node_barrier(spd, NULL);
            /* } else { */
            /*     warning("The number of MPI processes and processes registered in DLB differ." */
            /*             " TALP will not print any summary."); */
            /* } */
        }
#endif
    }
}

/* Decide whether to update the current sample (most likely and cheaper)
 * or to aggregate all samples.
 * We will only aggregate all samples if the external profiler is enabled
 * and this MPI call is a blocking collective call. */
static inline void update_sample_on_sync_call(const subprocess_descriptor_t *spd,
        const talp_info_t *talp_info, talp_sample_t *sample, bool is_blocking_collective) {

    if (!talp_info->flags.external_profiler || !is_blocking_collective) {
        /* Likely scenario, just update the sample */
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);
    } else {
        /* If talp_info->flags.external_profiler && is_blocking_collective:
         * aggregate samples and update all monitoring regions */
        talp_flush_samples_to_regions(spd);
    }
}

void talp_into_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        update_sample_on_sync_call(spd, talp_info, sample, is_blocking_collective);

        /* Into Sync call -> not_useful_mpi */
        talp_set_sample_state(sample, not_useful_mpi, talp_info->flags.papi);
    }
}

void talp_out_of_sync_call(const subprocess_descriptor_t *spd, bool is_blocking_collective) {
    /* Observer threads may call MPI functions, but TALP must ignore them */
    if (unlikely(thread_is_observer)) return;

    const talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update sample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_mpi_calls, 1);
        update_sample_on_sync_call(spd, talp_info, sample, is_blocking_collective);

        /* Out of Sync call -> useful */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}
