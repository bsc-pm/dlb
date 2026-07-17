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

#ifndef MNGO_METRICS_H
#define MNGO_METRICS_H

#include "mngo/mngo_talp.h"
#include "support/queues.h"

#ifndef MNGO_HISTORY_WINDOW
#define MNGO_HISTORY_WINDOW 256
#endif//MNGO_HISTORY_WINFOW

typedef struct {
    float self_pe;
    float shared_pe;
    float global_pe;
} mngo_reduced_metric_t;

typedef struct {
  queue_t *history;
} mngo_metrics_history_t;

void mngo_metrics__history_init(mngo_metrics_history_t *metrics_history);

void mngo_metrics__history_fini(mngo_metrics_history_t *metrics_history);

void mngo_metrics__history_push(mngo_metrics_history_t *metrics_history, const mngo_talp_metrics_t *metrics);

mngo_talp_metrics_t* mngo_metrics__history_emplace(mngo_metrics_history_t *metrics_history);

void mngo_metrics__history_tendency(const mngo_metrics_history_t *metrics_history,
                                        mngo_reduced_metric_t *metrics);

#endif /* MNGO_METRICS_H */
