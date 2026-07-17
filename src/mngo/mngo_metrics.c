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

#include "mngo/mngo_metrics.h"

#include "mngo/mngo_talp.h"
#include "support/queues.h"

void mngo_metrics__history_init(mngo_metrics_history_t *metrics_history) {
    metrics_history->history =
        queue__init(sizeof(mngo_talp_metrics_t), MNGO_HISTORY_WINDOW, NULL,
                    QUEUE_ALLOC_OVERWRITE);
}

void mngo_metrics__history_fini(mngo_metrics_history_t *metrics_history) {
    queue__destroy(metrics_history->history);
}

void mngo_metrics__history_push(mngo_metrics_history_t *metrics_history,
                                const mngo_talp_metrics_t *metrics) {
    queue__push_head(metrics_history->history, metrics);
}

mngo_talp_metrics_t *
mngo_metrics__history_emplace(mngo_metrics_history_t *metrics_history) {
    return queue__emplace_head(metrics_history->history);
}

struct mngo_reduce_metric_history_t {
    unsigned int coeficient_weight;
    unsigned int coeficient_sum;

    float self_mpi_parallel_efficiency;
    float shared_mpi_parallel_efficiency;
    float global_mpi_parallel_efficiency;
};

static void reduce_metric_history_op(void *args, void *curr) {
    struct mngo_reduce_metric_history_t *acc = args;
    mngo_talp_metrics_t *metrics = curr;

    acc->self_mpi_parallel_efficiency +=
        metrics->self_mpi_parallel_efficiency * acc->coeficient_weight;

    acc->shared_mpi_parallel_efficiency +=
        metrics->shared_mpi_parallel_efficiency * acc->coeficient_weight;

    acc->global_mpi_parallel_efficiency +=
        metrics->global_mpi_parallel_efficiency * acc->coeficient_weight;

    acc->coeficient_sum += acc->coeficient_weight;
    acc->coeficient_weight--;
}

void mngo_metrics__history_tendency(
    const mngo_metrics_history_t *metrics_history,
    mngo_reduced_metric_t *metrics) {
    struct mngo_reduce_metric_history_t historical_metrics = {
        .coeficient_weight = queue__get_size(metrics_history->history),
        .coeficient_sum = 0,

        .self_mpi_parallel_efficiency = 0,
        .shared_mpi_parallel_efficiency = 0,
        .global_mpi_parallel_efficiency = 0,
    };

    queue_iter_head2tail_t iter_metrics =
        queue__into_head2tail_iter(metrics_history->history);
    queue_iter__foreach(&iter_metrics, reduce_metric_history_op,
                        &historical_metrics);

    historical_metrics.self_mpi_parallel_efficiency /=
        historical_metrics.coeficient_sum;
    historical_metrics.shared_mpi_parallel_efficiency /=
        historical_metrics.coeficient_sum;
    historical_metrics.global_mpi_parallel_efficiency /=
        historical_metrics.coeficient_sum;

    metrics->self_pe = historical_metrics.self_mpi_parallel_efficiency;
    metrics->shared_pe = historical_metrics.shared_mpi_parallel_efficiency;
    metrics->global_pe = historical_metrics.global_mpi_parallel_efficiency;
}
