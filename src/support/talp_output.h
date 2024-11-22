/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#ifndef TALP_OUTPUT_H
#define TALP_OUTPUT_H

#include "apis/dlb_talp.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

typedef struct process_in_node_record_t {
    pid_t pid;
    int64_t mpi_time;
    int64_t useful_time;
} process_in_node_record_t;

typedef struct node_record_t {
    int node_id;
    int nelems;
    int64_t avg_useful_time;
    int64_t avg_mpi_time;
    int64_t max_useful_time;
    int64_t max_mpi_time;
    process_in_node_record_t processes[];
} node_record_t;

enum { TALP_OUTPUT_CPUSET_MAX = 128 };
typedef struct process_record_t {
    int rank;
    pid_t pid;
    int node_id;
    char hostname[HOST_NAME_MAX];
    char cpuset[TALP_OUTPUT_CPUSET_MAX];
    char cpuset_quoted[TALP_OUTPUT_CPUSET_MAX];
    dlb_monitor_t monitor;
} process_record_t;

void talp_output_print_monitoring_region(const dlb_monitor_t *monitor,
        const char *cpuset_str, bool have_mpi, bool have_openmp, bool have_papi);

void talp_output_record_pop_metrics(const dlb_pop_metrics_t *metrics);

void talp_output_record_resources(int num_cpus, int num_nodes, int num_mpi_ranks);

void talp_output_record_node(const node_record_t *node_record);

void talp_output_record_process(const char *monitor_name,
        const process_record_t *process_record, int num_mpi_ranks);

void talp_output_finalize(const char *output_file);

#endif /* TALP_OUTPUT_H */
