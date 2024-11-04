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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/talp_output.h"

#include "apis/dlb_talp.h"
#include "LB_core/DLB_talp.h"
#include "support/debug.h"
#include "support/gslist.h"
#include "support/mytime.h"
#include "support/perf_metrics.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>


/*********************************************************************************/
/*    Monitoring Region                                                          */
/*********************************************************************************/

void talp_output_print_monitoring_region(const dlb_monitor_t *monitor,
        const char *cpuset_str, bool have_mpi, bool have_openmp, bool have_papi) {

    char elapsed_time_str[16];
    ns_to_human(elapsed_time_str, 16, monitor->elapsed_time);

    info("################# Monitoring Region Summary #################");
    info("### Name:                                     %s", monitor->name);
    info("### Elapsed Time:                             %s", elapsed_time_str);
    info("### Useful time:                              %"PRId64" ns",
            monitor->useful_time);
    if (have_mpi) {
        info("### Not useful MPI:                           %"PRId64" ns",
                monitor->mpi_time);
    }
    if (have_openmp) {
        info("### Not useful OMP Load Balance:              %"PRId64" ns",
                monitor->omp_load_imbalance_time);
        info("### Not useful OMP Scheduling:                %"PRId64" ns",
                monitor->omp_scheduling_time);
        info("### Not useful OMP Serialization:             %"PRId64" ns",
                monitor->omp_serialization_time);
    }
    info("### CpuSet:                                   %s", cpuset_str);
    if (have_papi) {
        float ipc = monitor->cycles == 0 ? 0.0f
            : (float)monitor->instructions / monitor->cycles;
        info("### IPC:                                      %.2f ", ipc);
    }
    if (have_mpi) {
        info("### Number of MPI calls:                      %"PRId64,
                monitor->num_mpi_calls);
    }
    if (have_openmp) {
        info("### Number of OpenMP parallels:               %"PRId64,
                monitor->num_omp_parallels);
        info("### Number of OpenMP tasks:                   %"PRId64,
                monitor->num_omp_tasks);
    }
}


/*********************************************************************************/
/*    POP Metrics                                                                */
/*********************************************************************************/

static GSList *pop_metrics_records = NULL;

void talp_output_record_pop_metrics(const dlb_pop_metrics_t *metrics) {

    /* Copy structure */
    dlb_pop_metrics_t *new_record = malloc(sizeof(dlb_pop_metrics_t));
    *new_record = *metrics;

    /* Add record to list */
    pop_metrics_records = g_slist_prepend(pop_metrics_records, new_record);
}

static void pop_metrics_print(void) {

    for (GSList *node = pop_metrics_records;
            node != NULL;
            node = node->next) {

        dlb_pop_metrics_t *record = node->data;

        if (record->elapsed_time > 0) {

            float avg_ipc = record->cycles == 0.0 ? 0.0f : record->instructions / record->cycles;
            char elapsed_time_str[16];
            ns_to_human(elapsed_time_str, 16, record->elapsed_time);
            info("############### Monitoring Region POP Metrics ###############");
            info("### Name:                                     %s", record->name);
            info("### Elapsed Time:                             %s", elapsed_time_str);
            if (avg_ipc > 0.0f) {
                info("### Average IPC:                              %1.2f", avg_ipc);
            }
            if (record->mpi_parallel_efficiency > 0.0f &&
                    record->omp_parallel_efficiency > 0.0f) {
                info("### Parallel efficiency:                      %1.2f",
                        record->parallel_efficiency);
            }
            if (record->mpi_parallel_efficiency > 0.0f) {
                info("### MPI Parallel efficiency:                  %1.2f",
                        record->mpi_parallel_efficiency);
                info("###   - MPI Communication efficiency:         %1.2f",
                        record->mpi_communication_efficiency);
                info("###   - MPI Load Balance:                     %1.2f",
                        record->mpi_load_balance);
                info("###       - MPI Load Balance in:              %1.2f",
                        record->mpi_load_balance_in);
                info("###       - MPI Load Balance out:             %1.2f",
                        record->mpi_load_balance_out);
            }
            if (record->omp_parallel_efficiency > 0.0f) {
                info("### OpenMP Parallel efficiency:               %1.2f",
                        record->omp_parallel_efficiency);
                info("###   - OpenMP Load Balance:                  %1.2f",
                        record->omp_load_balance);
                info("###   - OpenMP Scheduling efficiency:         %1.2f",
                        record->omp_scheduling_efficiency);
                info("###   - OpenMP Serialization efficiency:      %1.2f",
                        record->omp_serialization_efficiency);
            }
        } else {
            info("############### Monitoring Region POP Metrics ###############");
            info("### Name:                                     %s", record->name);
            info("###                        No data                        ###");
        }
    }
}

static void pop_metrics_to_json(FILE *out_file) {

    if (pop_metrics_records != NULL) {
        fprintf(out_file,
                    "  \"popMetrics\": {\n");

        for (GSList *node = pop_metrics_records;
                node != NULL;
                node = node->next) {

            dlb_pop_metrics_t *record = node->data;

            fprintf(out_file,
                    "    \"%s\": {\n"
                    "      \"numCpus\": %d,\n"
                    "      \"numMpiRanks\": %d,\n"
                    "      \"numNodes\": %d,\n"
                    "      \"avgCpus\": %.1f,\n"
                    "      \"cycles\": %.0f,\n"
                    "      \"instructions\": %.0f,\n"
                    "      \"numMeasurements\": %"PRId64",\n"
                    "      \"numMpiCalls\": %"PRId64",\n"
                    "      \"numOmpParallels\": %"PRId64",\n"
                    "      \"numOmpTasks\": %"PRId64",\n"
                    "      \"elapsedTime\": %"PRId64",\n"
                    "      \"usefulTime\": %"PRId64",\n"
                    "      \"mpiTime\": %"PRId64",\n"
                    "      \"ompLoadImbalanceTime\": %"PRId64",\n"
                    "      \"ompSchedulingTime\": %"PRId64",\n"
                    "      \"ompSerializationTime\": %"PRId64",\n"
                    "      \"usefulNormdApp\": %.0f,\n"
                    "      \"mpiNormdApp\": %.0f,\n"
                    "      \"maxUsefulNormdProc\": %.0f,\n"
                    "      \"maxUsefulNormdNode\": %.0f,\n"
                    "      \"mpiNormdOfMaxUseful\": %.0f,\n"
                    "      \"parallelEfficiency\": %.2f,\n"
                    "      \"mpiParallelEfficiency\": %.2f,\n"
                    "      \"mpiCommunicationEfficiency\": %.2f,\n"
                    "      \"mpiLoadBalance\": %.2f,\n"
                    "      \"mpiLoadBalanceIn\": %.2f,\n"
                    "      \"mpiLoadBalanceOut\": %.2f,\n"
                    "      \"ompParallelEfficiency\": %.2f,\n"
                    "      \"ompLoadBalance\": %.2f,\n"
                    "      \"ompSchedulingEfficiency\": %.2f,\n"
                    "      \"ompSerializationEfficiency\": %.2f\n"
                    "    }%s\n",
                    record->name,
                    record->num_cpus,
                    record->num_mpi_ranks,
                    record->num_nodes,
                    record->avg_cpus,
                    record->cycles,
                    record->instructions,
                    record->num_measurements,
                    record->num_mpi_calls,
                    record->num_omp_parallels,
                    record->num_omp_tasks,
                    record->elapsed_time,
                    record->useful_time,
                    record->mpi_time,
                    record->omp_load_imbalance_time,
                    record->omp_scheduling_time,
                    record->omp_serialization_time,
                    record->useful_normd_app,
                    record->mpi_normd_app,
                    record->max_useful_normd_proc,
                    record->max_useful_normd_node,
                    record->mpi_normd_of_max_useful,
                    record->parallel_efficiency,
                    record->mpi_parallel_efficiency,
                    record->mpi_communication_efficiency,
                    record->mpi_load_balance,
                    record->mpi_load_balance_in,
                    record->mpi_load_balance_out,
                    record->omp_parallel_efficiency,
                    record->omp_load_balance,
                    record->omp_scheduling_efficiency,
                    record->omp_serialization_efficiency,
                    node->next != NULL ? "," : "");
        }
        fprintf(out_file,
                    "  }");         /* no eol */
    }
}

static void pop_metrics_to_xml(FILE *out_file) {

    for (GSList *node = pop_metrics_records;
            node != NULL;
            node = node->next) {

        dlb_pop_metrics_t *record = node->data;

        fprintf(out_file,
                "  <popMetrics>\n"
                "    <name>%s</name>\n"
                "    <numCpus>%d</numCpus>\n"
                "    <numMpiRanks>%d</numMpiRanks>\n"
                "    <numNodes>%d</numNodes>\n"
                "    <avgCpus>%.1f</avgCpus>\n"
                "    <cycles>%.0f</cycles>\n"
                "    <instructions>%.0f</instructions>\n"
                "    <numMeasurements>%"PRId64"</numMeasurements>\n"
                "    <numMpiCalls>%"PRId64"</numMpiCalls>\n"
                "    <numOmpParallels>%"PRId64"</numOmpParallels>\n"
                "    <numOmpTasks>%"PRId64"</numOmpTasks>\n"
                "    <elapsedTime>%"PRId64"</elapsedTime>\n"
                "    <usefulTime>%"PRId64"</usefulTime>\n"
                "    <mpiTime>%"PRId64"</mpiTime>\n"
                "    <ompLoadImbalanceTime>%"PRId64"</ompLoadImbalanceTime>\n"
                "    <ompSchedulingTime>%"PRId64"</ompSchedulingTime>\n"
                "    <ompSerializationTime>%"PRId64"</ompSerializationTime>\n"
                "    <usefulNormdApp>%.0f</usefulNormdApp>\n"
                "    <mpiNormdApp>%.0f</mpiNormdApp>\n"
                "    <maxUsefulNormdProc>%.0f</maxUsefulNormdProc>\n"
                "    <maxUsefulNormdNode>%.0f</maxUsefulNormdNode>\n"
                "    <mpiNormdOfMaxUseful>%.0f</mpiNormdOfMaxUseful>\n"
                "    <parallelEfficiency>%.2f</parallelEfficiency>\n"
                "    <mpiParallelEfficiency>%.2f</mpiParallelEfficiency>\n"
                "    <mpiCommunicationEfficiency>%.2f</mpiCommunicationEfficiency>\n"
                "    <mpiLoadBalance>%.2f</mpiLoadBalance>\n"
                "    <mpiLoadBalanceIn>%.2f</mpiLoadBalanceIn>\n"
                "    <mpiLoadBalanceOut>%.2f</mpiLoadBalanceOut>\n"
                "    <ompParallelEfficiency>%.2f</ompParallelEfficiency>\n"
                "    <ompLoadBalance>%.2f</ompLoadBalance>\n"
                "    <ompSchedulingEfficiency>%.2f</ompSchedulingEfficiency>\n"
                "    <ompSerializationEfficiency>%.2f</ompSerializationEfficiency>\n"
                "  </popMetrics>\n",
                record->name,
                record->num_cpus,
                record->num_mpi_ranks,
                record->num_nodes,
                record->avg_cpus,
                record->cycles,
                record->instructions,
                record->num_measurements,
                record->num_mpi_calls,
                record->num_omp_parallels,
                record->num_omp_tasks,
                record->elapsed_time,
                record->useful_time,
                record->mpi_time,
                record->omp_load_imbalance_time,
                record->omp_scheduling_time,
                record->omp_serialization_time,
                record->useful_normd_app,
                record->mpi_normd_app,
                record->max_useful_normd_proc,
                record->max_useful_normd_node,
                record->mpi_normd_of_max_useful,
                record->parallel_efficiency,
                record->mpi_parallel_efficiency,
                record->mpi_communication_efficiency,
                record->mpi_load_balance,
                record->mpi_load_balance_in,
                record->mpi_load_balance_out,
                record->omp_parallel_efficiency,
                record->omp_load_balance,
                record->omp_scheduling_efficiency,
                record->omp_serialization_efficiency
                );
    }
}

static void pop_metrics_to_txt(FILE *out_file) {

    for (GSList *node = pop_metrics_records;
            node != NULL;
            node = node->next) {

        dlb_pop_metrics_t *record = node->data;

        if (record->elapsed_time > 0) {
            fprintf(out_file,
                    "############### Monitoring Region POP Metrics ###############\n"
                    "### Name:                                      %s\n"
                    "### Number of CPUs:                            %d\n"
                    "### Number of MPI processes:                   %d\n"
                    "### Number of nodes:                           %d\n"
                    "### Average CPUs:                              %.1f\n"
                    "### Cycles:                                    %.0f\n"
                    "### Instructions:                              %.0f\n"
                    "### Number of measurements:                    %"PRId64"\n"
                    "### Number of MPI calls:                       %"PRId64"\n"
                    "### Number of OpenMP parallel regions:         %"PRId64"\n"
                    "### Number of OpenMP explicit tasks:           %"PRId64"\n"
                    "### Elapsed Time (ns):                         %"PRId64"\n"
                    "### Useful Time (ns):                          %"PRId64"\n"
                    "### MPI Time (ns):                             %"PRId64"\n"
                    "### OpenMP Load Imbalance Time (ns):           %"PRId64"\n"
                    "### OpenMP Scheduling Time (ns):               %"PRId64"\n"
                    "### OpenMP Serialization Time (ns):            %"PRId64"\n"
                    "### Useful Time normalized to App:             %.0f\n"
                    "### MPI Time normalized to App:                %.0f\n"
                    "### Maximum useful time across processes:      %.0f\n"
                    "### Maximum useful time across nodes:          %.0f\n"
                    "### MPI time normalized at process level of\n"
                    "###     the process with the max useful time:  %.0f\n"
                    "### Parallel efficiency:                       %.2f\n"
                    "### MPI Parallel efficiency:                   %.2f\n"
                    "###   - MPI Communication efficiency:          %.2f\n"
                    "###   - MPI Load Balance:                      %.2f\n"
                    "###       - MPI Load Balance in:               %.2f\n"
                    "###       - MPI Load Balance out:              %.2f\n"
                    "### OpenMP Parallel efficiency:                %.2f\n"
                    "###   - OpenMP Load Balance:                   %.2f\n"
                    "###   - OpenMP Scheduling efficiency:          %.2f\n"
                    "###   - OpenMP Serialization efficiency:       %.2f\n",
                    record->name,
                    record->num_cpus,
                    record->num_mpi_ranks,
                    record->num_nodes,
                    record->avg_cpus,
                    record->cycles,
                    record->instructions,
                    record->num_measurements,
                    record->num_mpi_calls,
                    record->num_omp_parallels,
                    record->num_omp_tasks,
                    record->elapsed_time,
                    record->useful_time,
                    record->mpi_time,
                    record->omp_load_imbalance_time,
                    record->omp_scheduling_time,
                    record->omp_serialization_time,
                    record->useful_normd_app,
                    record->mpi_normd_app,
                    record->max_useful_normd_proc,
                    record->max_useful_normd_node,
                    record->mpi_normd_of_max_useful,
                    record->parallel_efficiency,
                    record->mpi_parallel_efficiency,
                    record->mpi_communication_efficiency,
                    record->mpi_load_balance,
                    record->mpi_load_balance_in,
                    record->mpi_load_balance_out,
                    record->omp_parallel_efficiency,
                    record->omp_load_balance,
                    record->omp_scheduling_efficiency,
                    record->omp_serialization_efficiency
                );
        } else {
            fprintf(out_file,
                    "############### Monitoring Region POP Metrics ###############\n"
                    "### Name:                                     %s\n"
                    "###                        No data                        ###\n",
                    record->name);
        }
    }
}

static void pop_metrics_to_csv(FILE *out_file, bool append) {

    if (pop_metrics_records == NULL) return;

    if (!append) {
        /* Print header */
        fprintf(out_file,
                "name,"
                "numCpus,"
                "numMpiRanks,"
                "numNodes,"
                "avgCpus,"
                "cycles,"
                "instructions,"
                "numMeasurements,"
                "numMpiCalls,"
                "numOmpParallels,"
                "numOmpTasks,"
                "elapsedTime,"
                "usefulTime,"
                "mpiTime,"
                "ompLoadImbalanceTime,"
                "ompSchedulingTime,"
                "ompSerializationTime,"
                "usefulNormdApp,"
                "mpiNormdApp,"
                "maxUsefulNormdProc,"
                "maxUsefulNormdNode,"
                "mpiNormdOfMaxUseful,"
                "parallelEfficiency,"
                "mpiParallelEfficiency,"
                "mpiCommunicationEfficiency,"
                "mpiLoadBalance,"
                "mpiLoadBalanceIn,"
                "mpiLoadBalanceOut,"
                "ompParallelEfficiency,"
                "ompLoadBalance,"
                "ompSchedulingEfficiency,"
                "ompSerializationEfficiency\n"
            );
    }

    for (GSList *node = pop_metrics_records;
            node != NULL;
            node = node->next) {

        dlb_pop_metrics_t *record = node->data;

        fprintf(out_file,
                "\"%s\","    /* name */
                "%d,"        /* numCpus */
                "%d,"        /* numMpiRanks */
                "%d,"        /* numNodes */
                "%.1f,"      /* avgCpus */
                "%.0f,"      /* cycles */
                "%.0f,"      /* instructions */
                "%"PRId64"," /* numMeasurements */
                "%"PRId64"," /* numMpiRanks */
                "%"PRId64"," /* numOmpParallels */
                "%"PRId64"," /* numOmpTasks */
                "%"PRId64"," /* elapsedTime */
                "%"PRId64"," /* usefulTime */
                "%"PRId64"," /* mpiTime */
                "%"PRId64"," /* ompLoadImbalanceTime */
                "%"PRId64"," /* ompSchedulingTime */
                "%"PRId64"," /* ompSerializationTime */
                "%.0f,"      /* usefulNormdApp */
                "%.0f,"      /* mpiNormdApp */
                "%.0f,"      /* maxUsefulNormdProc */
                "%.0f,"      /* maxUsefulNormdNode */
                "%.0f,"      /* mpiNormdOfMaxUseful */
                "%.2f,"      /* parallelEfficiency */
                "%.2f,"      /* mpiParallelEfficiency */
                "%.2f,"      /* mpiCommunicationEfficiency */
                "%.2f,"      /* mpiLoadBalance */
                "%.2f,"      /* mpiLoadBalanceIn */
                "%.2f,"      /* mpiLoadBalanceOut */
                "%.2f,"      /* ompParallelEfficiency */
                "%.2f,"      /* ompLoadBalance */
                "%.2f,"      /* ompSchedulingEfficiency */
                "%.2f\n",    /* ompSerializationEfficiency */
                record->name,
                record->num_cpus,
                record->num_mpi_ranks,
                record->num_nodes,
                record->avg_cpus,
                record->cycles,
                record->instructions,
                record->num_measurements,
                record->num_mpi_calls,
                record->num_omp_parallels,
                record->num_omp_tasks,
                record->elapsed_time,
                record->useful_time,
                record->mpi_time,
                record->omp_load_imbalance_time,
                record->omp_scheduling_time,
                record->omp_serialization_time,
                record->useful_normd_app,
                record->mpi_normd_app,
                record->max_useful_normd_proc,
                record->max_useful_normd_node,
                record->mpi_normd_of_max_useful,
                record->parallel_efficiency,
                record->mpi_parallel_efficiency,
                record->mpi_communication_efficiency,
                record->mpi_load_balance,
                record->mpi_load_balance_in,
                record->mpi_load_balance_out,
                record->omp_parallel_efficiency,
                record->omp_load_balance,
                record->omp_scheduling_efficiency,
                record->omp_serialization_efficiency
            );
    }
}

static void pop_metrics_finalize(void) {

    /* Free every record data */
    for (GSList *node = pop_metrics_records;
            node != NULL;
            node = node->next) {

        dlb_pop_metrics_t *record = node->data;
        free(record);
    }

    /* Free list */
    g_slist_free(pop_metrics_records);
    pop_metrics_records = NULL;
}


/*********************************************************************************/
/*    Node                                                                       */
/*********************************************************************************/

static GSList *node_records = NULL;

void talp_output_record_node(const node_record_t *node_record) {

    int nelems = node_record->nelems;

    /* Allocate new record */
    size_t process_records_size = sizeof(process_in_node_record_t) * nelems;
    size_t node_record_size = sizeof(node_record_t) + process_records_size;
    node_record_t *new_record = malloc(node_record_size);

    /* Memcpy the entire struct */
    memcpy(new_record, node_record, node_record_size);

    /* Insert to list */
    node_records = g_slist_prepend(node_records, new_record);
}

static void node_print(void) {

    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *node_record = node->data;

        info(" |----------------------------------------------------------|");
        info(" |                  Extended Report Node %4d               |",
                node_record->node_id);
        info(" |----------------------------------------------------------|");
        info(" |  Process   |     Useful Time      |       MPI Time       |");
        info(" |------------|----------------------|----------------------|");

        for (int i = 0; i < node_record->nelems; ++i) {
            info(" | %-10d | %18e s | %18e s |",
                    node_record->processes[i].pid,
                    nsecs_to_secs(node_record->processes[i].useful_time),
                    nsecs_to_secs(node_record->processes[i].mpi_time));
            info(" |------------|----------------------|----------------------|");
        }

        if (node_record->nelems > 0) {
            info(" |------------|----------------------|----------------------|");
            info(" | %-10s | %18e s | %18e s |", "Node Avg",
                    nsecs_to_secs(node_record->avg_useful_time),
                    nsecs_to_secs(node_record->avg_mpi_time));
            info(" |------------|----------------------|----------------------|");
            info(" | %-10s | %18e s | %18e s |", "Node Max",
                    nsecs_to_secs(node_record->max_useful_time),
                    nsecs_to_secs(node_record->max_mpi_time));
            info(" |------------|----------------------|----------------------|");
        }
    }
}

static void node_to_json(FILE *out_file) {

    if (node_records == NULL) return;

    /* If there are pop_metrics, append to the existing dictionary */
    if (pop_metrics_records != NULL) {
        fprintf(out_file,",\n");
    }

    fprintf(out_file,
                "  \"node\": [\n");

    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *node_record = node->data;

        fprintf(out_file,
                "    {\n"
                "      \"id\": \"%d\",\n"
                "      \"process\": [\n",
                node_record->node_id);

        for (int i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                "        {\n"
                "          \"id\": %d,\n"
                "          \"usefulTime\": %"PRId64",\n"
                "          \"mpiTime\": %"PRId64"\n"
                "        }%s\n",
                node_record->processes[i].pid,
                node_record->processes[i].useful_time,
                node_record->processes[i].mpi_time,
                i+1 < node_record->nelems ? "," : "");
        }

        fprintf(out_file,
                "      ],\n"
                "      \"nodeAvg\": {\n"
                "        \"usefulTime\": %"PRId64",\n"
                "        \"mpiTime\": %"PRId64"\n"
                "      },\n"
                "      \"nodeMax\": {\n"
                "        \"usefulTime\": %"PRId64",\n"
                "        \"mpiTime\": %"PRId64"\n"
                "      }\n"
                "    }%s\n",
                node_record->avg_useful_time,
                node_record->avg_mpi_time,
                node_record->max_useful_time,
                node_record->max_mpi_time,
                node->next != NULL ? "," : "");
    }
    fprintf(out_file,
                "  ]");         /* no eol */
}

static void node_to_xml(FILE *out_file) {

    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *node_record = node->data;

        fprintf(out_file,
                "  <node>\n"
                "    <id>%d</id>\n",
                node_record->node_id);

        for (int i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                "    <process>\n"
                "      <id>%d</id>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "    </process>\n",
                node_record->processes[i].pid,
                node_record->processes[i].useful_time,
                node_record->processes[i].mpi_time);
        }

        fprintf(out_file,
                "    <nodeAvg>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "    </nodeAvg>\n"
                "    <nodeMax>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "    </nodeMax>\n"
                "  </node>\n",
                node_record->avg_useful_time,
                node_record->avg_mpi_time,
                node_record->max_useful_time,
                node_record->max_mpi_time);
    }
}

static void node_to_csv(FILE *out_file, bool append) {

    if (node_records == NULL) return;

    if (!append) {
        /* Print header */
        fprintf(out_file,
                "NodeId,"
                "ProcessId,"
                "ProcessUsefulTime,"
                "ProcessMPITime,"
                "NodeAvgUsefulTime,"
                "NodeAvgMPITime,"
                "NodeMaxUsefulTime,"
                "NodeMaxMPITime\n");
    }

    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *node_record = node->data;

        for (int i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                    "%d,"           /* NodeId */
                    "%d,"           /* ProcessId */
                    "%"PRId64","    /* ProcessUsefulTime */
                    "%"PRId64","    /* ProcessMPITime */
                    "%"PRId64","    /* NodeAvgUsefulTime */
                    "%"PRId64","    /* NodeAvgMPITime*/
                    "%"PRId64","    /* NodeMaxUsefulTime */
                    "%"PRId64"\n",  /* NodeMaxMPITime*/
                    node_record->node_id,
                    node_record->processes[i].pid,
                    node_record->processes[i].useful_time,
                    node_record->processes[i].mpi_time,
                    node_record->avg_useful_time,
                    node_record->avg_mpi_time,
                    node_record->max_useful_time,
                    node_record->max_mpi_time);

        }
    }
}

static void node_to_txt(FILE *out_file) {

    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *node_record = node->data;

        fprintf(out_file,
                " |----------------------------------------------------------|\n"
                " |                  Extended Report Node %4d               |\n"
                " |----------------------------------------------------------|\n"
                " |  Process   |     Useful Time      |       MPI Time       |\n"
                " |------------|----------------------|----------------------|\n",
                node_record->node_id);

        for (int i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                " | %-10d | %18e s | %18e s |\n"
                " |------------|----------------------|----------------------|\n",
                node_record->processes[i].pid,
                nsecs_to_secs(node_record->processes[i].useful_time),
                nsecs_to_secs(node_record->processes[i].mpi_time));
        }

        if (node_record->nelems > 0) {
            fprintf(out_file,
                " |------------|----------------------|----------------------|\n"
                " | %-10s | %18e s | %18e s |\n"
                " |------------|----------------------|----------------------|\n"
                " | %-10s | %18e s | %18e s |\n"
                " |------------|----------------------|----------------------|\n",
                    "Node Avg",
                    nsecs_to_secs(node_record->avg_useful_time),
                    nsecs_to_secs(node_record->avg_mpi_time),
                    "Node Max",
                    nsecs_to_secs(node_record->max_useful_time),
                    nsecs_to_secs(node_record->max_mpi_time));
        }
    }
}

static void node_finalize(void) {

    /* Free every record data */
    for (GSList *node = node_records;
            node != NULL;
            node = node->next) {

        node_record_t *record = node->data;
        free(record);
    }

    /* Free list */
    g_slist_free(node_records);
    node_records = NULL;
}


/*********************************************************************************/
/*    Process                                                                    */
/*********************************************************************************/

typedef struct region_record_t {
    char name[DLB_MONITOR_NAME_MAX];
    int num_mpi_ranks;
    process_record_t process_records[];
} region_record_t;

static GSList *region_records = NULL;

void talp_output_record_process(const char *region_name,
        const process_record_t *process_record, int num_mpi_ranks) {

     region_record_t *region_record = NULL;

    /* Find region or allocate new one */
    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record = node->data;
        if (strcmp(region_record->name, region_name) == 0) {
            break;
        }
    }

    /* Allocate if not found */
    if (region_record == NULL) {
        /* Allocate and initialize new region */
        size_t region_record_size = sizeof(region_record_t) +
            sizeof(process_record_t) * num_mpi_ranks;
        region_record = malloc(region_record_size);
        *region_record = (const region_record_t) {
            .num_mpi_ranks = num_mpi_ranks,
        };
        snprintf(region_record->name, DLB_MONITOR_NAME_MAX, "%s",
                region_name);

        /* Insert to list */
        region_records = g_slist_prepend(region_records, region_record);
    }

    /* Copy process_record */
    int rank = process_record->rank;
    ensure(rank < num_mpi_ranks, "Wrong rank number in %s", __func__);
    memcpy(&region_record->process_records[rank], process_record, sizeof(process_record_t));
}

static void process_print(void) {

    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *region_record = node->data;

        for (int i = 0; i < region_record->num_mpi_ranks; ++i) {

            process_record_t *process_record = &region_record->process_records[i];

            info("################# Monitoring Region Summary ##################");
            info("### Name:                                     %s",
                    region_record->name);
            info("### Process:                                  %d (%s)",
                    process_record->pid, process_record->hostname);
            info("### Rank:                                     %d",
                    process_record->rank);
            info("### CpuSet:                                   %s",
                    process_record->cpuset);
            info("### Elapsed time:                             %"PRId64" ns",
                    process_record->monitor.elapsed_time);
            info("### Useful time:                              %"PRId64" ns",
                    process_record->monitor.useful_time);
            if (process_record->monitor.mpi_time > 0) {
                info("### Not useful MPI:                           %"PRId64" ns",
                        process_record->monitor.mpi_time);
            }
            if (process_record->monitor.omp_load_imbalance_time > 0
                    || process_record->monitor.omp_scheduling_time > 0
                    || process_record->monitor.omp_serialization_time > 0) {
                info("### Not useful OMP Load Imbalance:              %"PRId64" ns",
                        process_record->monitor.omp_load_imbalance_time);
                info("### Not useful OMP Scheduling:                %"PRId64" ns",
                        process_record->monitor.omp_scheduling_time);
                info("### Not useful OMP Serialization:             %"PRId64" ns",
                        process_record->monitor.omp_serialization_time);
            }
            if (process_record->monitor.cycles > 0) {
                info("### IPC :                                     %.2f",
                        (float)process_record->monitor.instructions
                        / process_record->monitor.cycles);
            }
        }
    }
}

static void process_to_json(FILE *out_file) {

    if (region_records == NULL) return;

    /* If there are pop_metrics or node_metrics, append to the existing dictionary */
    if (pop_metrics_records != NULL
            || node_records != NULL) {
        fprintf(out_file,",\n");
    }

    fprintf(out_file,
                "  \"region\": [\n");

    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *region_record = node->data;

        fprintf(out_file,
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"process\": [\n",
                region_record->name);

        for (int i = 0; i < region_record->num_mpi_ranks; ++i) {

            process_record_t *process_record = &region_record->process_records[i];

            fprintf(out_file,
                "        {\n"
                "          \"rank\": %d,\n"
                "          \"pid\": %d,\n"
                "          \"hostname\": \"%s\",\n"
                "          \"cpuset\": %s,\n"
                "          \"numCpus\": %d,\n"
                "          \"avgCpus\": %.1f,\n"
                "          \"cycles\": %"PRId64",\n"
                "          \"instructions\": %"PRId64",\n"
                "          \"numMeasurements\": %d,\n"
                "          \"numResets\": %d,\n"
                "          \"numMpiCalls\": %"PRId64",\n"
                "          \"numOmpParallels\": %"PRId64",\n"
                "          \"numOmpTasks\": %"PRId64",\n"
                "          \"elapsedTime\": %"PRId64",\n"
                "          \"usefulTime\": %"PRId64",\n"
                "          \"mpiTime\": %"PRId64",\n"
                "          \"ompLoadImbalanceTime\": %"PRId64",\n"
                "          \"ompSchedulingTime\": %"PRId64",\n"
                "          \"ompSerializationTime\": %"PRId64"\n"
                "        }%s\n",
                process_record->rank,
                process_record->pid,
                process_record->hostname,
                process_record->cpuset_quoted,
                process_record->monitor.num_cpus,
                process_record->monitor.avg_cpus,
                process_record->monitor.cycles,
                process_record->monitor.instructions,
                process_record->monitor.num_measurements,
                process_record->monitor.num_resets,
                process_record->monitor.num_mpi_calls,
                process_record->monitor.num_omp_parallels,
                process_record->monitor.num_omp_tasks,
                process_record->monitor.elapsed_time,
                process_record->monitor.useful_time,
                process_record->monitor.mpi_time,
                process_record->monitor.omp_load_imbalance_time,
                process_record->monitor.omp_scheduling_time,
                process_record->monitor.omp_serialization_time,
                i + 1 < region_record->num_mpi_ranks ? "," : "");
        }
        fprintf(out_file,
                "      ]\n"
                "    }%s\n",
                node->next != NULL ? "," : "");
    }
    fprintf(out_file,
                "  ]");         /* no eol */
}

static void process_to_xml(FILE *out_file) {

    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *region_record = node->data;

        fprintf(out_file,
                "  <region>\n"
                "    <name>%s</name>\n",
                region_record->name);

        for (int i = 0; i < region_record->num_mpi_ranks; ++i) {

            process_record_t *process_record = &region_record->process_records[i];

            fprintf(out_file,
                "    <process>\n"
                "      <rank>%d</rank>\n"
                "      <pid>%d</pid>\n"
                "      <hostname>%s</hostname>\n"
                "      <cpuset>%s</cpuset>\n"
                "      <numCpus>%d</numCpus>\n"
                "      <avgCpus>%.1f</avgCpus>\n"
                "      <cycles>%"PRId64"</cycles>\n"
                "      <instructions>%"PRId64"</instructions>\n"
                "      <numMeasurements>%d</numMeasurements>\n"
                "      <numResets>%d</numResets>\n"
                "      <numMpiCalls>%"PRId64"</numMpiCalls>\n"
                "      <numOmpParallels>%"PRId64"</numOmpParallels>\n"
                "      <numOmpTasks>%"PRId64"</numOmpTasks>\n"
                "      <elapsedTime>%"PRId64"</elapsedTime>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "      <ompLoadImbalanceTime>%"PRId64"</ompLoadImbalanceTime>\n"
                "      <ompSchedulingTime>%"PRId64"</ompSchedulingTime>\n"
                "      <ompSerializationTime>%"PRId64"</ompSerializationTime>\n"
                "    </process>\n",
                process_record->rank,
                process_record->pid,
                process_record->hostname,
                process_record->cpuset_quoted,
                process_record->monitor.num_cpus,
                process_record->monitor.avg_cpus,
                process_record->monitor.cycles,
                process_record->monitor.instructions,
                process_record->monitor.num_measurements,
                process_record->monitor.num_resets,
                process_record->monitor.num_mpi_calls,
                process_record->monitor.num_omp_parallels,
                process_record->monitor.num_omp_tasks,
                process_record->monitor.elapsed_time,
                process_record->monitor.useful_time,
                process_record->monitor.mpi_time,
                process_record->monitor.omp_load_imbalance_time,
                process_record->monitor.omp_scheduling_time,
                process_record->monitor.omp_serialization_time);
        }
        fprintf(out_file,
                "  </region>\n");
    }
}

static void process_to_csv(FILE *out_file, bool append) {

    if (region_records == NULL) return;

    if (!append) {
        /* Print header */
        fprintf(out_file,
                "Region,"
                "Rank,"
                "PID,"
                "Hostname,"
                "CpuSet,"
                "NumCpus,"
                "AvgCpus,"
                "Cycles,"
                "Instructions,"
                "NumMeasurements,"
                "NumResets,"
                "NumMpiCalls,"
                "NumOmpParallels,"
                "NumOmpTasks,"
                "ElapsedTime,"
                "UsefulTime,"
                "MPITime,"
                "OMPLoadImbalance,"
                "OMPSchedulingTime,"
                "OMPSerializationTime\n");
    }

    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *region_record = node->data;

        for (int i = 0; i < region_record->num_mpi_ranks; ++i) {

            process_record_t *process_record = &region_record->process_records[i];

            fprintf(out_file,
                    "%s,"           /* Region */
                    "%d,"           /* Rank */
                    "%d,"           /* PID */
                    "%s,"           /* Hostname */
                    "%s,"           /* CpuSet */
                    "%d"            /* NumCpus */
                    "%.1f,"         /* AvgCpus */
                    "%"PRId64","    /* Cycles */
                    "%"PRId64","    /* Instructions */
                    "%d,"           /* NumMeasurements */
                    "%d,"           /* NumResets */
                    "%"PRId64","    /* NumMpiCalls */
                    "%"PRId64","    /* NumOmpParallels */
                    "%"PRId64","    /* NumOmpTasks */
                    "%"PRId64","    /* ElapsedTime */
                    "%"PRId64","    /* UsefulTime */
                    "%"PRId64","    /* MPITime */
                    "%"PRId64","    /* OMPLoadImbalance */
                    "%"PRId64","    /* OMPSchedulingTime */
                    "%"PRId64",",   /* OMPSerializationTime */
                    region_record->name,
                    process_record->rank,
                    process_record->pid,
                    process_record->hostname,
                    process_record->cpuset_quoted,
                    process_record->monitor.num_cpus,
                    process_record->monitor.avg_cpus,
                    process_record->monitor.cycles,
                    process_record->monitor.instructions,
                    process_record->monitor.num_measurements,
                    process_record->monitor.num_resets,
                    process_record->monitor.num_mpi_calls,
                    process_record->monitor.num_omp_parallels,
                    process_record->monitor.num_omp_tasks,
                    process_record->monitor.elapsed_time,
                    process_record->monitor.useful_time,
                    process_record->monitor.mpi_time,
                    process_record->monitor.omp_load_imbalance_time,
                    process_record->monitor.omp_scheduling_time,
                    process_record->monitor.omp_serialization_time);
        }
    }
}

static void process_to_txt(FILE *out_file) {

    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *region_record = node->data;

        for (int i = 0; i < region_record->num_mpi_ranks; ++i) {

            process_record_t *process_record = &region_record->process_records[i];

            float ipc = process_record->monitor.cycles > 0
                ?  (float)process_record->monitor.instructions
                    / process_record->monitor.cycles
                : 0.0f;

            fprintf(out_file,
                    "################# Monitoring Region Summary ##################\n"
                    "### Name:                                     %s\n"
                    "### Process:                                  %d (%s)\n"
                    "### Rank:                                     %d\n"
                    "### CpuSet:                                   %s\n"
                    "### Elapsed time:                             %"PRId64" ns\n"
                    "### Useful time:                              %"PRId64" ns\n"
                    "### Not useful MPI:                           %"PRId64" ns\n"
                    "### Not useful OMP Load Imbalance:            %"PRId64" ns\n"
                    "### Not useful OMP Scheduling:                %"PRId64" ns\n"
                    "### Not useful OMP Serialization:             %"PRId64" ns\n"
                    "### IPC:                                      %.2f\n",
                    region_record->name,
                    process_record->pid, process_record->hostname,
                    process_record->rank,
                    process_record->cpuset,
                    process_record->monitor.elapsed_time,
                    process_record->monitor.useful_time,
                    process_record->monitor.mpi_time,
                    process_record->monitor.omp_load_imbalance_time,
                    process_record->monitor.omp_scheduling_time,
                    process_record->monitor.omp_serialization_time,
                    ipc);
        }
    }
}

static void process_finalize(void) {

    /* Free every record data */
    for (GSList *node = region_records;
            node != NULL;
            node = node->next) {

        region_record_t *record = node->data;
        free(record);
    }

    /* Free list */
    g_slist_free(region_records);
    region_records = NULL;
}


/*********************************************************************************/
/*    TALP Common                                                                */
/*********************************************************************************/
typedef struct TALPCommonRecord {
    char *time_of_creation; // ISO 8601 string
    char *dlb_major_version; // Major.Minor DLB version used
    char *dlb_git_description; // GIT description output
} talp_common_record_t;
static talp_common_record_t common_record;

static void talp_output_record_common(void) {
    /* Initialize structure */
    time_t now = time(NULL);
    common_record = (const talp_common_record_t) {
        .time_of_creation = get_iso_8601_string(localtime(&now)),
        .dlb_major_version = PACKAGE_VERSION,
        .dlb_git_description = DLB_GIT_DESCRIPTION,
    };
}

static void common_to_json(FILE *out_file) {
    fprintf(out_file,
                    "  \"dlbVersion\": \"%s\",\n"
                    "  \"dlbGitVersion\": \"%s\",\n"
                    "  \"timestamp\": \"%s\",\n",
                common_record.dlb_major_version,
                common_record.dlb_git_description,
                common_record.time_of_creation);
}

static void common_to_xml(FILE *out_file) {

    fprintf(out_file,
            "  <dlbVersion>%s</dlbVersion>\n"
            "  <dlbGitVersion>%s</dlbGitVersion>\n"
            "  <timestamp>%s</timestamp>\n",
            common_record.dlb_major_version,
            common_record.dlb_git_description,
            common_record.time_of_creation);
}

static void common_to_txt(FILE *out_file) {

    fprintf(out_file,
            "################ TALP Common Data ################\n"
            "### DLB Version:                   %s\n"
            "### DLB Git Version:               %s\n"
            "### Timestamp:                     %s\n",
            common_record.dlb_major_version,
            common_record.dlb_git_description,
            common_record.time_of_creation);
}

static void common_finalize(void) {
    free(common_record.time_of_creation);
}




/*********************************************************************************/
/*    TALP Resources                                                             */
/*********************************************************************************/
typedef struct TALPResourcesRecord {
    unsigned int num_cpus;
    unsigned int num_nodes;
    unsigned int num_mpi_ranks;
} talp_resources_record_t;
static talp_resources_record_t resources_record;

void talp_output_record_resources(int num_cpus, int num_nodes, int num_mpi_ranks) {

    resources_record = (const talp_resources_record_t) {
        .num_cpus = (unsigned int) num_cpus,
        .num_nodes = (unsigned int) num_nodes,
        .num_mpi_ranks = (unsigned int) num_mpi_ranks
    };
}

static void resources_to_json(FILE *out_file) {
    fprintf(out_file,
                    "  \"resources\": {\n"
                    "    \"numCpus\": %u,\n"
                    "    \"numNodes\": %u,\n"
                    "    \"numMpiRanks\": %u\n"
                    "  },\n",
                resources_record.num_cpus,
                resources_record.num_nodes,
                resources_record.num_mpi_ranks);
}

static void resources_to_xml(FILE *out_file) {

    fprintf(out_file,
            "  <resources>\n"
            "    <numCpus>%u</numCpus>\n"
            "    <numNodes>%u</numNodes>\n"
            "    <numMpiRanks>%u</numMpiRanks>\n"
            "  </resources>",
            resources_record.num_cpus,
            resources_record.num_nodes,
            resources_record.num_mpi_ranks);
}

static void resources_to_txt(FILE *out_file) {

    fprintf(out_file,
            "################# TALP Resources #################\n"
            "### Number of CPUs:                %u\n"
            "### Number of Nodes:               %u\n"
            "### Number of MPI processes:       %u\n",
            resources_record.num_cpus,
            resources_record.num_nodes,
            resources_record.num_mpi_ranks);
}


/*********************************************************************************/
/*    Helper functions                                                           */
/*********************************************************************************/

static void json_header(FILE *out_file) {
    fprintf(out_file, "{\n");
}

static void json_footer(FILE *out_file) {
    fprintf(out_file, "\n}\n");
}

static void xml_header(FILE *out_file) {
    fprintf(out_file, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                      "<root>\n");
}

static void xml_footer(FILE *out_file) {
    fprintf(out_file, "</root>\n");
}


/*********************************************************************************/
/*    Finalize                                                                   */
/*********************************************************************************/

void talp_output_finalize(const char *output_file) {

    /* For efficiency, all records are prepended to their respective lists and
     * reversed here */
    pop_metrics_records = g_slist_reverse(pop_metrics_records);
    node_records        = g_slist_reverse(node_records);
    region_records      = g_slist_reverse(region_records);

    talp_output_record_common();

    if (output_file == NULL) {
        /* No output file, just print all records */
        pop_metrics_print();
        node_print();
        process_print();
    } else {
        /* Do not open file if process has no data */
        if (pop_metrics_records == NULL
                && node_records == NULL
                && region_records == NULL) return;

        /* Check file extension */
        typedef enum Extension {
            EXT_JSON,
            EXT_XML,
            EXT_CSV,
            EXT_TXT,
        } extension_t;
        extension_t extension = EXT_TXT;
        const char *ext = strrchr(output_file, '.');
        if (ext != NULL) {
            if (strcmp(ext+1, "json") == 0) {
                extension = EXT_JSON;
            } else if (strcmp(ext+1, "xml") == 0) {
                extension = EXT_XML;
            } else if (strcmp(ext+1, "csv") == 0) {
                extension = EXT_CSV;
            }
        }

        /* Deprecation warning*/
        if(extension == EXT_XML){
            warning("Deprecated: The support for XML output is deprecated and"
                    " will be removed in the next release");
        }

        /* Specific case where output file needs to be split */
        if (extension == EXT_CSV
                && !!(pop_metrics_records != NULL)
                    + !!(node_records != NULL)
                    + !!(region_records != NULL) > 1) {

            /* Length without extension */
            int filename_useful_len = ext - output_file;

            /* POP */
            if (pop_metrics_records != NULL) {
                const char *pop_ext = "-pop.csv";
                size_t pop_file_len = filename_useful_len + strlen(pop_ext) + 1;
                char *pop_filename = malloc(sizeof(char)*pop_file_len);
                sprintf(pop_filename, "%.*s%s", filename_useful_len, output_file, pop_ext);
                FILE *pop_file;
                bool append_to_csv;
                if (access(pop_filename, F_OK) == 0) {
                    pop_file = fopen(pop_filename, "a");
                    append_to_csv = true;
                } else {
                    pop_file = fopen(pop_filename, "w");
                    append_to_csv = false;
                }
                if (pop_file == NULL) {
                    warning("Cannot open file %s: %s", pop_filename, strerror(errno));
                } else {
                    pop_metrics_to_csv(pop_file, append_to_csv);
                    fclose(pop_file);
                }
            }

            /* Node */
            if (node_records != NULL) {
                const char *node_ext = "-node.csv";
                size_t node_file_len = filename_useful_len + strlen(node_ext) + 1;
                char *node_filename = malloc(sizeof(char)*node_file_len);
                sprintf(node_filename, "%.*s%s", filename_useful_len, output_file, node_ext);
                FILE *node_file;
                bool append_to_csv;
                if (access(node_filename, F_OK) == 0) {
                    node_file = fopen(node_filename, "a");
                    append_to_csv = true;
                } else {
                    node_file = fopen(node_filename, "w");
                    append_to_csv = false;
                }
                if (node_file == NULL) {
                    warning("Cannot open file %s: %s", node_filename, strerror(errno));
                } else {
                    node_to_csv(node_file, append_to_csv);
                    fclose(node_file);
                }
            }

            /* Process */
            if (region_records != NULL) {
                const char *process_ext = "-process.csv";
                size_t process_file_len = filename_useful_len + strlen(process_ext) + 1;
                char *process_filename = malloc(sizeof(char)*process_file_len);
                sprintf(process_filename, "%.*s%s", filename_useful_len, output_file, process_ext);
                FILE *process_file;
                bool append_to_csv;
                if (access(process_filename, F_OK) == 0) {
                    process_file = fopen(process_filename, "a");
                    append_to_csv = true;
                } else {
                    process_file = fopen(process_filename, "w");
                    append_to_csv = false;
                }
                if (process_file == NULL) {
                    warning("Cannot open file %s: %s", process_filename, strerror(errno));
                } else {
                    process_to_csv(process_file, append_to_csv);
                    fclose(process_file);
                }
            }
        }

        /* Write to file */
        else {
            /* Open file */
            FILE *out_file;
            bool append_to_csv;
            if (extension == EXT_CSV
                    && access(output_file, F_OK) == 0) {
                /* Specific case where new entries are appended to existing csv */
                out_file = fopen(output_file, "a");
                append_to_csv = true;
            } else {
                out_file = fopen(output_file, "w");
                append_to_csv = false;
            }
            if (out_file == NULL) {
                warning("Cannot open file %s: %s", output_file, strerror(errno));
            } else {
                /* Write records to file */
                switch(extension) {
                    case EXT_JSON:
                        json_header(out_file);
                        common_to_json(out_file);
                        resources_to_json(out_file);
                        pop_metrics_to_json(out_file);
                        node_to_json(out_file);
                        process_to_json(out_file);
                        json_footer(out_file);
                        break;
                    case EXT_XML:
                        xml_header(out_file);
                        common_to_xml(out_file);
                        resources_to_xml(out_file);
                        pop_metrics_to_xml(out_file);
                        node_to_xml(out_file);
                        process_to_xml(out_file);
                        xml_footer(out_file);
                        break;
                    case EXT_CSV:
                        pop_metrics_to_csv(out_file, append_to_csv);
                        node_to_csv(out_file, append_to_csv);
                        process_to_csv(out_file, append_to_csv);
                        break;
                    case EXT_TXT:
                        common_to_txt(out_file);
                        resources_to_txt(out_file);
                        pop_metrics_to_txt(out_file);
                        node_to_txt(out_file);
                        process_to_txt(out_file);
                        break;
                }
                /* Close file */
                fclose(out_file);
            }
        }
    }

    // De-allocate all records
    common_finalize();
    pop_metrics_finalize();
    node_finalize();
    process_finalize();
}
