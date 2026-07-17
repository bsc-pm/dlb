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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include <assert.h>
#include <float.h>
#include <unistd.h>
#include <stdbool.h>

#include "mngo/mngo_metrics.h"

static inline bool compare_float_eq(float a, float b) {
    float residual_ab = a - b;
    float residual_ba = b - a;
    
    return residual_ab <= FLT_EPSILON && residual_ba <= FLT_EPSILON;
}

int main(int argc, char *argv[]) {
    // Easy test on push
    {
        mngo_metrics_history_t history;
        mngo_metrics__history_init(&history);
    
        mngo_talp_metrics_t metric_00 = (const mngo_talp_metrics_t) {
            .self_mpi_parallel_efficiency = 0.1010101010,
            .shared_mpi_parallel_efficiency = 0.2020202020,
            .global_mpi_parallel_efficiency = 0.3030303030,
        };
        mngo_metrics__history_push(&history, &metric_00);

        mngo_reduced_metric_t reduced_00;
        mngo_metrics__history_tendency(&history, &reduced_00);
        assert(compare_float_eq(metric_00.self_mpi_parallel_efficiency,reduced_00.self_pe));
        assert(compare_float_eq(metric_00.shared_mpi_parallel_efficiency,reduced_00.shared_pe));
        assert(compare_float_eq(metric_00.global_mpi_parallel_efficiency,reduced_00.global_pe));

        mngo_metrics__history_fini(&history);
    }

    // Easy test on emplace
    {
        mngo_metrics_history_t history;
        mngo_metrics__history_init(&history);
    
        mngo_talp_metrics_t *metric_00 = mngo_metrics__history_emplace(&history);
        metric_00->self_mpi_parallel_efficiency = 0.1010101010;
        metric_00->shared_mpi_parallel_efficiency = 0.2020202020;
        metric_00->global_mpi_parallel_efficiency = 0.3030303030;

        mngo_reduced_metric_t reduced_00;
        mngo_metrics__history_tendency(&history, &reduced_00);
        assert(compare_float_eq(metric_00->self_mpi_parallel_efficiency, reduced_00.self_pe));
        assert(compare_float_eq(metric_00->shared_mpi_parallel_efficiency, reduced_00.shared_pe));
        assert(compare_float_eq(metric_00->global_mpi_parallel_efficiency, reduced_00.global_pe));

        mngo_metrics__history_fini(&history);
    }

    // Check multiple entries
    {
        mngo_metrics_history_t history;
        mngo_metrics__history_init(&history);
    
        for (int i = 0; i < MNGO_HISTORY_WINDOW * 2; i++) {
            mngo_talp_metrics_t *metric_00 = mngo_metrics__history_emplace(&history);
            metric_00->self_mpi_parallel_efficiency = 0.1010101010;
            metric_00->shared_mpi_parallel_efficiency = 0.2020202020;
            metric_00->global_mpi_parallel_efficiency = 0.3030303030;
        }

        mngo_talp_metrics_t metric_00 = (const mngo_talp_metrics_t) {
            .self_mpi_parallel_efficiency = 0.1010101010,
            .shared_mpi_parallel_efficiency = 0.2020202020,
            .global_mpi_parallel_efficiency = 0.3030303030,
        };

        mngo_reduced_metric_t reduced_00;
        mngo_metrics__history_tendency(&history, &reduced_00);
        assert(compare_float_eq(metric_00.self_mpi_parallel_efficiency, reduced_00.self_pe));
        assert(compare_float_eq(metric_00.shared_mpi_parallel_efficiency, reduced_00.shared_pe));
        assert(compare_float_eq(metric_00.global_mpi_parallel_efficiency, reduced_00.global_pe));

        mngo_metrics__history_fini(&history);
    }

    // Check multiple entries and replace everything
    {
        mngo_metrics_history_t history;
        mngo_metrics__history_init(&history);

        for (int i = 0; i < MNGO_HISTORY_WINDOW; i++) {
            mngo_talp_metrics_t *metric_00 = mngo_metrics__history_emplace(&history);
            metric_00->self_mpi_parallel_efficiency = 0.9341234320;
            metric_00->shared_mpi_parallel_efficiency = 0.2342549324;
            metric_00->global_mpi_parallel_efficiency = 0.3141235321;
        }
    
        for (int i = 0; i < MNGO_HISTORY_WINDOW; i++) {
            mngo_talp_metrics_t *metric_00 = mngo_metrics__history_emplace(&history);
            metric_00->self_mpi_parallel_efficiency = 0.1010101010;
            metric_00->shared_mpi_parallel_efficiency = 0.2020202020;
            metric_00->global_mpi_parallel_efficiency = 0.3030303030;
        }

        mngo_talp_metrics_t metric_00 = (const mngo_talp_metrics_t) {
            .self_mpi_parallel_efficiency = 0.1010101010,
            .shared_mpi_parallel_efficiency = 0.2020202020,
            .global_mpi_parallel_efficiency = 0.3030303030,
        };

        mngo_reduced_metric_t reduced_00;
        mngo_metrics__history_tendency(&history, &reduced_00);
        assert(compare_float_eq(metric_00.self_mpi_parallel_efficiency, reduced_00.self_pe));
        assert(compare_float_eq(metric_00.shared_mpi_parallel_efficiency, reduced_00.shared_pe));
        assert(compare_float_eq(metric_00.global_mpi_parallel_efficiency, reduced_00.global_pe));

        mngo_metrics__history_fini(&history);
    }

    // Check weigted average
    //  - the oldests weights 1
    //  - and the yourngest wieghts #entries
    {
        mngo_metrics_history_t history;
        mngo_metrics__history_init(&history);

        mngo_talp_metrics_t *metric_00 = mngo_metrics__history_emplace(&history);
        metric_00->self_mpi_parallel_efficiency = 0.5;
        metric_00->shared_mpi_parallel_efficiency = 0.4;
        metric_00->global_mpi_parallel_efficiency = 0.3;

        mngo_talp_metrics_t *metric_01 = mngo_metrics__history_emplace(&history);
        metric_01->self_mpi_parallel_efficiency = 0.4;
        metric_01->shared_mpi_parallel_efficiency = 0.3;
        metric_01->global_mpi_parallel_efficiency = 0.2;

        mngo_talp_metrics_t *metric_02 = mngo_metrics__history_emplace(&history);
        metric_02->self_mpi_parallel_efficiency = 0.3;
        metric_02->shared_mpi_parallel_efficiency = 0.2;
        metric_02->global_mpi_parallel_efficiency = 0.1;

        mngo_talp_metrics_t *metric_03 = mngo_metrics__history_emplace(&history);
        metric_03->self_mpi_parallel_efficiency = 0.2;
        metric_03->shared_mpi_parallel_efficiency = 0.1;
        metric_03->global_mpi_parallel_efficiency = 0.0;

        mngo_reduced_metric_t expected = (const mngo_reduced_metric_t) {
            .self_pe = 0.3,
            .shared_pe = 0.2,
            .global_pe = 0.1,
        };

        mngo_reduced_metric_t reduced_00;
        mngo_metrics__history_tendency(&history, &reduced_00);
        assert(compare_float_eq(expected.self_pe, reduced_00.self_pe));
        assert(compare_float_eq(expected.shared_pe, reduced_00.shared_pe));
        assert(compare_float_eq(expected.global_pe, reduced_00.global_pe));

        mngo_metrics__history_fini(&history);
    }
    
}
