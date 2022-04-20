/*********************************************************************************/
/*  Copyright 2009-2022 Barcelona Supercomputing Center                          */
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

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

typedef struct ProcessInNodeRecord {
    pid_t pid;
    int64_t mpi_time;
    int64_t useful_time;
} process_in_node_record_t;

void talp_output_record_pop_metrics(const char *name, int64_t elapsed_time,
        float parallel_efficiency, float communication_efficiency,
        float lb, float lb_in, float lb_out);

void talp_output_record_pop_raw(const char *name, int P, int N, int64_t elapsed_time,
        int64_t elapsed_useful, int64_t app_sum_useful, int64_t node_sum_useful);

void talp_output_record_node(int node_id, int nelems, int64_t avg_useful_time,
        int64_t avg_mpi_time, int64_t max_useful_time, int64_t max_mpi_time,
        process_in_node_record_t *process_info);

void talp_output_record_process(const char *monitor_name, int rank, pid_t pid,
        int num_measurements, const char* hostname, const char *cpuset,
        const char *cpuset_quoted, int64_t elapsed_time, int64_t elapsed_useful_time,
        int64_t accumulated_MPI_time, int64_t accumulated_useful_time);

void talp_output_finalize(const char *output_file);

#endif /* TALP_OUTPUT_H */
