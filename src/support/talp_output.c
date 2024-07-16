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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/talp_output.h"

#include "apis/dlb_talp.h"
#include "LB_core/DLB_talp.h"
#include "support/debug.h"
#include "support/mytime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>


/*********************************************************************************/
/*    POP Metrics                                                                */
/*********************************************************************************/
typedef struct POPMetricsRecord {
    char *name;
    int64_t elapsed_time;
    float avg_ipc;
    float parallel_efficiency;
    float communication_efficiency;
    float lb;
    float lb_in;
    float lb_out;
} pop_metrics_record_t;
static int pop_metrics_num_records = 0;
static pop_metrics_record_t *pop_metrics = NULL;

void talp_output_record_pop_metrics(const char *name, int64_t elapsed_time,
        float avg_ipc, float parallel_efficiency, float communication_efficiency,
        float lb, float lb_in, float lb_out) {

    /* Realloc array */
    ++pop_metrics_num_records;
    void *ptr = realloc(pop_metrics,
            pop_metrics_num_records*sizeof(pop_metrics_record_t));
    fatal_cond(!ptr, "realloc failed");
    pop_metrics = ptr;

    /* Allocate monitor name */
    char *allocated_name = malloc(DLB_MONITOR_NAME_MAX*sizeof(char));
    snprintf(allocated_name, DLB_MONITOR_NAME_MAX, "%s", name);

    /* Initialize structure */
    pop_metrics[pop_metrics_num_records-1] = (const pop_metrics_record_t) {
        .name = allocated_name,
        .elapsed_time = elapsed_time,
        .avg_ipc = avg_ipc,
        .parallel_efficiency = parallel_efficiency,
        .communication_efficiency = communication_efficiency,
        .lb = lb,
        .lb_in = lb_in,
        .lb_out = lb_out,
    };
}

static void pop_metrics_print(void) {
    int i;
    for (i=0; i<pop_metrics_num_records; ++i) {
        pop_metrics_record_t *record = &pop_metrics[i];
        if (record->elapsed_time > 0) {
            char elapsed_time_str[16];
            ns_to_human(elapsed_time_str, 16, record->elapsed_time);
            info("######### Monitoring Region POP Metrics #########");
            info("### Name:                       %s", record->name);
            info("### Elapsed Time :              %s", elapsed_time_str);
            if (record->avg_ipc > 0.0f) {
                info("### Average IPC :               %1.2f", record->avg_ipc);
            }
            info("### Parallel efficiency :       %1.2f", record->parallel_efficiency);
            info("###   - Communication eff. :    %1.2f", record->communication_efficiency);
            info("###   - Load Balance :          %1.2f", record->lb);
            info("###       - LB_in :             %1.2f", record->lb_in);
            info("###       - LB_out :            %1.2f", record->lb_out);
        } else {
            info("######### Monitoring Region POP Metrics #########");
            info("### Name:                       %s", record->name);
            info("###                  No data                  ###");
        }
    }
}

static void pop_metrics_to_json(FILE *out_file) {
    if (pop_metrics_num_records > 0) {
        fprintf(out_file,
                    "  \"popMetrics\": [\n");
        int i;
        for (i=0; i<pop_metrics_num_records; ++i) {
            pop_metrics_record_t *record = &pop_metrics[i];
            fprintf(out_file,
                    "    {\n"
                    "      \"name\": \"%s\",\n"
                    "      \"elapsedTime\": %"PRId64",\n"
                    "      \"averageIPC\": %.2f,\n"
                    "      \"parallelEfficiency\": %.2f,\n"
                    "      \"communicationEfficiency\": %.2f,\n"
                    "      \"loadBalance\": %.2f,\n"
                    "      \"lbIn\": %.2f,\n"
                    "      \"lbOut\": %.2f\n"
                    "    }%s\n",
                    record->name,
                    record->elapsed_time,
                    record->avg_ipc,
                    record->parallel_efficiency,
                    record->communication_efficiency,
                    record->lb,
                    record->lb_in,
                    record->lb_out,
                    i+1 < pop_metrics_num_records ? "," : "");
        }
        fprintf(out_file,
                    "  ]");         /* no eol */
    }
}

static void pop_metrics_to_xml(FILE *out_file) {
    int i;
    for (i=0; i<pop_metrics_num_records; ++i) {
        pop_metrics_record_t *record = &pop_metrics[i];
        fprintf(out_file,
                "  <popMetrics>\n"
                "    <name>%s</name>\n"
                "    <elapsedTime>%"PRId64"</elapsedTime>\n"
                "    <averageIPC>%.2f</averageIPC>\n"
                "    <parallelEfficiency>%.2f</parallelEfficiency>\n"
                "    <communicationEfficiency>%.2f</communicationEfficiency>\n"
                "    <loadBalance>%.2f</loadBalance>\n"
                "    <lbIn>%.2f</lbIn>\n"
                "    <lbOut>%.2f</lbOut>\n"
                "  </popMetrics>\n",
                record->name,
                record->elapsed_time,
                record->avg_ipc,
                record->parallel_efficiency,
                record->communication_efficiency,
                record->lb,
                record->lb_in,
                record->lb_out);
    }
}

static void pop_metrics_to_txt(FILE *out_file) {
    int i;
    for (i=0; i<pop_metrics_num_records; ++i) {
        pop_metrics_record_t *record = &pop_metrics[i];
        if (record->elapsed_time > 0) {
            char elapsed_time_str[16];
            ns_to_human(elapsed_time_str, 16, record->elapsed_time);
            fprintf(out_file,
                    "######### Monitoring Region POP Metrics #########\n"
                    "### Name:                       %s\n"
                    "### Elapsed Time :              %s\n"
                    "### Average IPC :               %1.2f\n"
                    "### Parallel efficiency :       %1.2f\n"
                    "###   - Communication eff. :    %1.2f\n"
                    "###   - Load Balance :          %1.2f\n"
                    "###       - LB_in :             %1.2f\n"
                    "###       - LB_out :            %1.2f\n",
                    record->name,
                    elapsed_time_str,
                    record->avg_ipc,
                    record->parallel_efficiency,
                    record->communication_efficiency,
                    record->lb,
                    record->lb_in,
                    record->lb_out);
        } else {
            fprintf(out_file,
                    "######### Monitoring Region POP Metrics #########\n"
                    "### Name:                       %s\n"
                    "###                  No data                  ###\n",
                    record->name);
        }
    }
}

static void pop_metrics_finalize(void) {
    int i;
    for (i=0; i<pop_metrics_num_records; ++i) {
        free(pop_metrics[i].name);
    }
    free(pop_metrics);
    pop_metrics = NULL;
    pop_metrics_num_records = 0;
}


/*********************************************************************************/
/*    POP Raw                                                                    */
/*********************************************************************************/
typedef struct POPRawRecord {
    char *name;
    int P;
    int N;
    int num_ranks;
    int64_t elapsed_time;
    int64_t elapsed_useful;
    int64_t app_sum_useful;
    int64_t node_sum_useful;
    int64_t num_mpi_calls;
    int64_t cycles;
    int64_t instructions;
} pop_raw_record_t;
static int pop_raw_num_records = 0;
static pop_raw_record_t *pop_raw = NULL;

void talp_output_record_pop_raw(const char *name, int P, int N, int num_ranks,
        int64_t elapsed_time, int64_t elapsed_useful, int64_t app_sum_useful,
        int64_t node_sum_useful, int64_t num_mpi_calls, int64_t cycles,
        int64_t instructions) {

    /* Realloc array */
    ++pop_raw_num_records;
    void *ptr = realloc(pop_raw, pop_raw_num_records*sizeof(pop_raw_record_t));
    fatal_cond(!ptr, "realloc failed");
    pop_raw = ptr;

    /* Allocate monitor name */
    char *allocated_name = malloc(DLB_MONITOR_NAME_MAX*sizeof(char));
    snprintf(allocated_name, DLB_MONITOR_NAME_MAX, "%s", name);

    /* Initialize structure */
    pop_raw[pop_raw_num_records-1] = (const pop_raw_record_t) {
        .name = allocated_name,
        .P = P,
        .N = N,
        .num_ranks = num_ranks,
        .elapsed_time = elapsed_time,
        .elapsed_useful = elapsed_useful,
        .app_sum_useful = app_sum_useful,
        .node_sum_useful = node_sum_useful,
        .num_mpi_calls = num_mpi_calls,
        .cycles = cycles,
        .instructions = instructions,
    };
}

static void pop_raw_print(void) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        pop_raw_record_t *record = &pop_raw[i];
        if (record->elapsed_time > 0) {
            info("######### Monitoring Region POP Raw Data #########");
            info("### Name:                         %s", record->name);
            info("### Number of CPUs:               %d", record->P);
            info("### Number of nodes:              %d", record->N);
            info("### Number of ranks:              %d", record->num_ranks);
            info("### Elapsed Time:                 %"PRId64" ns", record->elapsed_time);
            info("### Elapsed Useful:               %"PRId64" ns", record->elapsed_useful);
            info("### Useful CPU Time (Total):      %"PRId64" ns", record->app_sum_useful);
            info("### Useful CPU Time (Node):       %"PRId64" ns", record->node_sum_useful);
            info("### Number of MPI calls (Total):  %"PRId64, record->num_mpi_calls);
            info("### Number of cycles (Total):     %"PRId64, record->cycles);
            info("### Number of instructions (T.):  %"PRId64, record->instructions);
        }
    }
}

static void pop_raw_to_json(FILE *out_file) {
    if (pop_raw_num_records > 0) {
        if (pop_metrics_num_records > 0) {
            fprintf(out_file,",\n");
        }
        fprintf(out_file,
                    "  \"popRaw\": [\n");
        int i;
        for (i=0; i<pop_raw_num_records; ++i) {
            pop_raw_record_t *record = &pop_raw[i];
            fprintf(out_file,
                    "    {\n"
                    "      \"name\": \"%s\",\n"
                    "      \"numCpus\": %d,\n"
                    "      \"numNodes\": %d,\n"
                    "      \"numRanks\": %d,\n"
                    "      \"elapsedTime\": %"PRId64",\n"
                    "      \"elapsedUseful\": %"PRId64",\n"
                    "      \"usefulCpuTotal\": %"PRId64",\n"
                    "      \"usefulCpuNode\": %"PRId64",\n"
                    "      \"numMPICalls\": %"PRId64",\n"
                    "      \"cycles\": %"PRId64",\n"
                    "      \"instructions\": %"PRId64"\n"
                    "    }%s\n",
                    record->name,
                    record->P,
                    record->N,
                    record->num_ranks,
                    record->elapsed_time,
                    record->elapsed_useful,
                    record->app_sum_useful,
                    record->node_sum_useful,
                    record->num_mpi_calls,
                    record->cycles,
                    record->instructions,
                    i+1 < pop_raw_num_records ? "," : "");
        }
        fprintf(out_file,
                    "  ]");         /* no eol */
    }
}

static void pop_raw_to_xml(FILE *out_file) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        pop_raw_record_t *record = &pop_raw[i];
        fprintf(out_file,
                "  <popRaw>\n"
                "    <name>%s</name>\n"
                "    <numCpus>%d</numCpus>\n"
                "    <numNodes>%d</numNodes>\n"
                "    <numRanks>%d</numRanks>\n"
                "    <elapsedTime>%"PRId64"</elapsedTime>\n"
                "    <elapsedUseful>%"PRId64"</elapsedUseful>\n"
                "    <usefulCpuTotal>%"PRId64"</usefulCpuTotal>\n"
                "    <usefulCpuNode>%"PRId64"</usefulCpuNode>\n"
                "    <numMPICalls>%"PRId64"</numMPICalls>\n"
                "    <cycles>%"PRId64"</cycles>\n"
                "    <instructions>%"PRId64"</instructions>\n"
                "  </popRaw>\n",
                record->name,
                record->P,
                record->N,
                record->num_ranks,
                record->elapsed_time,
                record->elapsed_useful,
                record->app_sum_useful,
                record->node_sum_useful,
                record->num_mpi_calls,
                record->cycles,
                record->instructions);
    }
}

static void pop_raw_to_txt(FILE *out_file) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        pop_raw_record_t *record = &pop_raw[i];
        if (record->elapsed_time > 0) {
            fprintf(out_file,
                    "######### Monitoring Region POP Raw Data #########\n"
                    "### Name:                          %s\n"
                    "### Number of CPUs:                %d\n"
                    "### Number of nodes:               %d\n"
                    "### Number of ranks:               %d\n"
                    "### Elapsed Time:                  %"PRId64" ns\n"
                    "### Elapsed Useful:                %"PRId64" ns\n"
                    "### Useful CPU Time (Total):       %"PRId64" ns\n"
                    "### Useful CPU Time (Node):        %"PRId64" ns\n"
                    "### Number of MPI calls (Total):   %"PRId64"\n"
                    "### Number of cycles (Total):      %"PRId64"\n"
                    "### Number of instructions (T.):   %"PRId64"\n",
                    record->name,
                    record->P,
                    record->N,
                    record->num_ranks,
                    record->elapsed_time,
                    record->elapsed_useful,
                    record->app_sum_useful,
                    record->node_sum_useful,
                    record->num_mpi_calls,
                    record->cycles,
                    record->instructions);
        }
    }
}

static void pop_raw_finalize(void) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        free(pop_raw[i].name);
    }
    free(pop_raw);
    pop_raw = NULL;
    pop_raw_num_records = 0;
}

/*********************************************************************************/
/*    POP Metrics + POP Raw CSV                                                  */
/*********************************************************************************/

static void pop_to_csv(FILE *out_file, bool append) {
    int num_records = max_int(pop_metrics_num_records, pop_raw_num_records);
    if (num_records == 0)
        return;

    if (!append) {
        /* Print header */
        fprintf(out_file, "Name,ElapsedTime,IPC%s%s\n",
                pop_metrics_num_records == 0 ? "" :
                ",ParallelEfficiency,CommunicationEfficiency,LoadBalance,LbIn,LbOut",
                pop_raw_num_records == 0 ? "" :
                ",NumCpus,NumNodes,NumRanks,ElapsedUseful,UsefulCpuTotal"
                ",UsefulCpuNode,numMPICalls,cycles,instructions"
               );
    }

    int i;
    for (i=0; i<num_records; ++i) {
        pop_metrics_record_t *metrics_record =
            i < pop_metrics_num_records ? &pop_metrics[i] : NULL;
        pop_raw_record_t *raw_record =
            i < pop_raw_num_records ? &pop_raw[i] : NULL;

        float ipc = metrics_record ? metrics_record->avg_ipc :
            raw_record->cycles > 0 ? (float)raw_record->instructions / raw_record->cycles : 0.0f;

        /* Assuming that if both records are not NULL, they are the same region */
        fprintf(out_file, "%s,%"PRId64",%1.2f",
                metrics_record ? metrics_record->name : raw_record->name,
                metrics_record ? metrics_record->elapsed_time : raw_record->elapsed_time,
                ipc);

        if (metrics_record) {
            fprintf(out_file, ",%.2f,%.2f,%.2f,%.2f,%.2f",
                    metrics_record->parallel_efficiency,
                    metrics_record->communication_efficiency,
                    metrics_record->lb,
                    metrics_record->lb_in,
                    metrics_record->lb_out);
        }

        if (raw_record) {
            fprintf(out_file,
                    /* P, N, num_ranks */
                    ",%d,%d,%d"
                    /* 3 useful */
                    ",%"PRId64",%"PRId64",%"PRId64
                    /* num_mpi_calls, cycles, instructions */
                    ",%"PRIu64",%"PRIu64",%"PRIu64,
                    raw_record->P,
                    raw_record->N,
                    raw_record->num_ranks,
                    raw_record->elapsed_useful,
                    raw_record->app_sum_useful,
                    raw_record->node_sum_useful,
                    raw_record->num_mpi_calls,
                    raw_record->cycles,
                    raw_record->instructions);
        }

        fprintf(out_file, "\n");
    }
}


/*********************************************************************************/
/*    Node                                                                       */
/*********************************************************************************/
typedef struct NodeRecord {
    int node_id;
    int nelems;
    int64_t avg_useful_time;
    int64_t avg_mpi_time;
    int64_t max_useful_time;
    int64_t max_mpi_time;
    struct ProcessInNodeRecord *process;
    struct NodeRecord *next;
} node_record_t;
static node_record_t *node_list_head = NULL;
static node_record_t *node_list_tail = NULL;

void talp_output_record_node(int node_id, int nelems, int64_t avg_useful_time,
        int64_t avg_mpi_time, int64_t max_useful_time, int64_t max_mpi_time,
        process_in_node_record_t *process_info) {

    /* Allocate processes record and memcpy the entire array */
    size_t process_record_size = sizeof(process_in_node_record_t) * nelems;
    process_in_node_record_t *process_record = malloc(process_record_size);
    memcpy(process_record, process_info, process_record_size);

    /* Allocate node record and initialize */
    node_record_t *node_record = malloc(sizeof(node_record_t));
    *node_record = (const node_record_t) {
        .node_id = node_id,
        .nelems = nelems,
        .avg_useful_time = avg_useful_time,
        .avg_mpi_time = avg_mpi_time,
        .max_useful_time = max_useful_time,
        .max_mpi_time = max_mpi_time,
        .process = process_record,
        .next = NULL
    };

    /* Insert to list */
    if (node_list_head == NULL) {
        node_list_head = node_record;
    } else {
        node_list_tail->next = node_record;
    }
    node_list_tail = node_record;
}

static void node_print(void) {
    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        info(" |----------------------------------------------------------|");
        info(" |                  Extended Report Node %4d               |",
                node_record->node_id);
        info(" |----------------------------------------------------------|");
        info(" |  Process   |     Useful Time      |       MPI Time       |");
        info(" |------------|----------------------|----------------------|");
        int i;
        for (i = 0; i < node_record->nelems; ++i) {
            info(" | %-10d | %18e s | %18e s |",
                    node_record->process[i].pid,
                    nsecs_to_secs(node_record->process[i].useful_time),
                    nsecs_to_secs(node_record->process[i].mpi_time));
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
        node_record = node_record->next;
    }
}

static void node_to_json(FILE *out_file) {
    if (node_list_head == NULL)
        return;

    if (pop_metrics_num_records + pop_raw_num_records > 0) {
        fprintf(out_file,",\n");
    }
    fprintf(out_file,
                "  \"node\": [\n");
    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        fprintf(out_file,
                "    {\n"
                "      \"id\": \"%d\",\n"
                "      \"process\": [\n",
                node_record->node_id);
        int i;
        for (i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                "        {\n"
                "          \"id\": %d,\n"
                "          \"usefulTime\": %"PRId64",\n"
                "          \"mpiTime\": %"PRId64"\n"
                "        }%s\n",
                node_record->process[i].pid,
                node_record->process[i].useful_time,
                node_record->process[i].mpi_time,
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
                node_record->next != NULL ? "," : "");

        node_record = node_record->next;
    }
    fprintf(out_file,
                "  ]");         /* no eol */
}

static void node_to_xml(FILE *out_file) {
    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        fprintf(out_file,
                "  <node>\n"
                "    <id>%d</id>\n",
                node_record->node_id);
        int i;
        for (i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                "    <process>\n"
                "      <id>%d</id>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "    </process>\n",
                node_record->process[i].pid,
                node_record->process[i].useful_time,
                node_record->process[i].mpi_time);
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

        node_record = node_record->next;
    }
}

static void node_to_csv(FILE *out_file, bool append) {
    if (node_list_head == NULL)
        return;

    if (!append) {
        /* Print header */
        fprintf(out_file,
                "NodeId,ProcessId,ProcessUsefulTime,ProcessMPITime,NodeAvgUsefulTime"
                ",NodeAvgMPITime,NodeMaxUsefulTime,NodeMaxMPITime\n");
    }

    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        int i;
        for (i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                    "%d,%d,%"PRId64",%"PRId64",%"PRId64",%"PRId64",%"PRId64",%"PRId64"\n",
                    node_record->node_id,
                    node_record->process[i].pid,
                    node_record->process[i].useful_time,
                    node_record->process[i].mpi_time,
                    node_record->avg_useful_time,
                    node_record->avg_mpi_time,
                    node_record->max_useful_time,
                    node_record->max_mpi_time);

        }
        node_record = node_record->next;
    }
}

static void node_to_txt(FILE *out_file) {
    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        fprintf(out_file,
                " |----------------------------------------------------------|\n"
                " |                  Extended Report Node %4d               |\n"
                " |----------------------------------------------------------|\n"
                " |  Process   |     Useful Time      |       MPI Time       |\n"
                " |------------|----------------------|----------------------|\n",
                node_record->node_id);
        int i;
        for (i = 0; i < node_record->nelems; ++i) {
            fprintf(out_file,
                " | %-10d | %18e s | %18e s |\n"
                " |------------|----------------------|----------------------|\n",
                node_record->process[i].pid,
                nsecs_to_secs(node_record->process[i].useful_time),
                nsecs_to_secs(node_record->process[i].mpi_time));
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
        node_record = node_record->next;
    }
}

static void node_finalize(void) {
    node_record_t *node_record = node_list_head;
    while (node_record != NULL) {
        node_record_t *next = node_record->next;
        free(node_record->process);
        free(node_record);
        node_record = next;
    }
    node_list_head = NULL;
    node_list_tail = NULL;
}


/*********************************************************************************/
/*    Process                                                                    */
/*********************************************************************************/
enum { CPUSET_MAX = 64 };
typedef struct ProcessRecord {
    int     rank;
    pid_t   pid;
    int     num_measurements;
    char    hostname[HOST_NAME_MAX];
    char    cpuset[CPUSET_MAX];
    char    cpuset_quoted[CPUSET_MAX];
    int64_t elapsed_time;
    int64_t elapsed_useful_time;
    int64_t accumulated_MPI_time;
    int64_t accumulated_useful_time;
    float  ipc;
    struct ProcessRecord *next;
} process_record_t;

typedef struct MonitorList {
    char monitor_name[DLB_MONITOR_NAME_MAX];
    process_record_t *process_list_head;
    process_record_t *process_list_tail;
    struct MonitorList *next;
} monitor_list_t;

static monitor_list_t *monitor_list_head = NULL;
static monitor_list_t *monitor_list_tail = NULL;

void talp_output_record_process(const char *monitor_name, int rank, pid_t pid,
        int num_measurements, const char* hostname, const char *cpuset,
        const char *cpuset_quoted, int64_t elapsed_time, int64_t elapsed_useful_time,
        int64_t accumulated_MPI_time, int64_t accumulated_useful_time, float ipc) {

    /* Find monitor list or allocate new one */
    monitor_list_t *monitor_list = monitor_list_head;
    while(monitor_list != NULL) {
        if (strcmp(monitor_list->monitor_name, monitor_name) == 0)
            break;
        monitor_list = monitor_list->next;
    }

    if (monitor_list == NULL) {
        /* Allocate new monitor */
        monitor_list = malloc(sizeof(monitor_list_t));
        *monitor_list = (const monitor_list_t) {
            .process_list_head = NULL,
            .process_list_tail = NULL,
        };
        snprintf(monitor_list->monitor_name, DLB_MONITOR_NAME_MAX, "%s", monitor_name);

        /* Insert to list */
        if (monitor_list_head == NULL) {
            monitor_list_head = monitor_list;
        } else {
            monitor_list_tail->next = monitor_list;
        }
        monitor_list_tail = monitor_list;
    }

    /* Allocate new process record */
    process_record_t *process_record = malloc(sizeof(process_record_t));
    *process_record = (const process_record_t) {
        .rank = rank,
        .pid = pid,
        .num_measurements = num_measurements,
        .elapsed_time = elapsed_time,
        .elapsed_useful_time = elapsed_useful_time,
        .accumulated_MPI_time = accumulated_MPI_time,
        .accumulated_useful_time = accumulated_useful_time,
        .ipc = ipc,
        .next = NULL,
    };
    snprintf(process_record->hostname, HOST_NAME_MAX, "%s", hostname);
    snprintf(process_record->cpuset, CPUSET_MAX, "%s", cpuset);
    snprintf(process_record->cpuset_quoted, CPUSET_MAX, "%s", cpuset_quoted);

    /* Add record to monitor list */
    if (monitor_list->process_list_head == NULL) {
        monitor_list->process_list_head = process_record;
    } else {
        monitor_list->process_list_tail->next = process_record;
    }
    monitor_list->process_list_tail = process_record;
}

static void process_print(void) {
    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            info("########### Monitoring Region Summary ###########");
            info("### Name:                       %s", monitor_list->monitor_name);
            info("### Process:                    %d (%s)", process_record->pid, process_record->hostname);
            info("### Rank:                       %d", process_record->rank);
            info("### CpuSet:                     %s", process_record->cpuset);
            info("### Elapsed time :              %.9g seconds",
                    nsecs_to_secs(process_record->elapsed_time));
            info("### Elapsed useful time :       %.9g seconds",
                    nsecs_to_secs(process_record->elapsed_useful_time));
            info("### MPI time :                  %.9g seconds",
                    nsecs_to_secs(process_record->accumulated_MPI_time));
            info("### Useful time :               %.9g seconds",
                    nsecs_to_secs(process_record->accumulated_useful_time));
            info("### IPC :                       %.2f", process_record->ipc);
            process_record = process_record->next;
        }
        monitor_list = monitor_list->next;;
    }
}

static void process_to_json(FILE *out_file) {
    if (monitor_list_head == NULL)
        return;

    if (pop_metrics_num_records + pop_raw_num_records > 0
            || node_list_head != NULL) {
        fprintf(out_file,",\n");
    }
    fprintf(out_file,
                "  \"region\": [\n");
    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        fprintf(out_file,
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"process\": [\n",
                monitor_list->monitor_name);
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            fprintf(out_file,
                "        {\n"
                "          \"rank\": %d,\n"
                "          \"pid\": %d,\n"
                "          \"hostname\": \"%s\",\n"
                "          \"cpuset\": %s,\n"
                "          \"numMeasurements\": %d,\n"
                "          \"elapsedTime\": %"PRId64",\n"
                "          \"elapsedUseful\": %"PRId64",\n"
                "          \"mpiTime\": %"PRId64",\n"
                "          \"usefulTime\": %"PRId64",\n"
                "          \"IPC\": %f\n"
                "        }%s\n",
                process_record->rank,
                process_record->pid,
                process_record->hostname,
                process_record->cpuset_quoted,
                process_record->num_measurements,
                process_record->elapsed_time,
                process_record->elapsed_useful_time,
                process_record->accumulated_MPI_time,
                process_record->accumulated_useful_time,
                process_record->ipc,
                process_record->next != NULL ? "," : "");
            process_record = process_record->next;
        }
        fprintf(out_file,
                "      ]\n"
                "    }%s\n",
                monitor_list->next != NULL ? "," : "");
        monitor_list = monitor_list->next;
    }
    fprintf(out_file,
                "  ]");         /* no eol */
}

static void process_to_xml(FILE *out_file) {
    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        fprintf(out_file,
                "  <region>\n"
                "    <name>%s</name>\n",
                monitor_list->monitor_name);
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            fprintf(out_file,
                "    <process>\n"
                "      <rank>%d</rank>\n"
                "      <pid>%d</pid>\n"
                "      <hostname>%s</hostname>\n"
                "      <cpuset>%s</cpuset>\n"
                "      <numMeasurements>%d</numMeasurements>\n"
                "      <elapsedTime>%"PRId64"</elapsedTime>\n"
                "      <elapsedUseful>%"PRId64"</elapsedUseful>\n"
                "      <mpiTime>%"PRId64"</mpiTime>\n"
                "      <usefulTime>%"PRId64"</usefulTime>\n"
                "      <IPC>%f</IPC>\n"
                "    </process>\n",
                process_record->rank,
                process_record->pid,
                process_record->hostname,
                process_record->cpuset_quoted,
                process_record->num_measurements,
                process_record->elapsed_time,
                process_record->elapsed_useful_time,
                process_record->accumulated_MPI_time,
                process_record->accumulated_useful_time,
                process_record->ipc);
            process_record = process_record->next;
        }
        fprintf(out_file,
                "  </region>\n");
        monitor_list = monitor_list->next;;
    }
}

static void process_to_csv(FILE *out_file, bool append) {
    if (monitor_list_head == NULL)
        return;

    if (!append) {
        /* Print header */
        fprintf(out_file,
                "Region,Rank,PID,Hostname,CpuSet,NumMeasurements"
                ",ElapsedTime,ElapsedUsefulTime,MPITime,UsefulTime,IPC\n");
    }

    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            fprintf(out_file,
                    "%s,%d,%d,%s,%s,%d,%"PRId64",%"PRId64",%"PRId64",%"PRId64",%f\n",
                    monitor_list->monitor_name,
                    process_record->rank,
                    process_record->pid,
                    process_record->hostname,
                    process_record->cpuset_quoted,
                    process_record->num_measurements,
                    process_record->elapsed_time,
                    process_record->elapsed_useful_time,
                    process_record->accumulated_MPI_time,
                    process_record->accumulated_useful_time,
                    process_record->ipc);
            process_record = process_record->next;
        }
        monitor_list = monitor_list->next;;
    }
}

static void process_to_txt(FILE *out_file) {
    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            fprintf(out_file,
                    "########### Monitoring Region Summary ###########\n"
                    "### Name:                      %s\n"
                    "### Process:                   %d (%s)\n"
                    "### Rank:                      %d\n"
                    "### CpuSet:                    %s\n"
                    "### Elapsed time :             %.9g seconds\n"
                    "### Elapsed useful time :      %.9g seconds\n"
                    "### MPI time :                 %.9g seconds\n"
                    "### Useful time :              %.9g seconds\n"
                    "### IPC:                       %.2f\n",
                    monitor_list->monitor_name,
                    process_record->pid, process_record->hostname,
                    process_record->rank,
                    process_record->cpuset,
                    nsecs_to_secs(process_record->elapsed_time),
                    nsecs_to_secs(process_record->elapsed_useful_time),
                    nsecs_to_secs(process_record->accumulated_MPI_time),
                    nsecs_to_secs(process_record->accumulated_useful_time),
                    process_record->ipc);
            process_record = process_record->next;
        }
        monitor_list = monitor_list->next;
    }
}

static void process_finalize(void) {
    monitor_list_t *monitor_list = monitor_list_head;
    while (monitor_list != NULL) {
        monitor_list_t *next_monitor_list = monitor_list->next;
        process_record_t *process_record = monitor_list->process_list_head;
        while (process_record != NULL) {
            process_record_t *next_process = process_record->next;
            free(process_record);
            process_record = next_process;
        }
        free(monitor_list);
        monitor_list = next_monitor_list;
    }
    monitor_list_head = NULL;
    monitor_list_tail = NULL;
}


/*********************************************************************************/
/*    TALP Metadata                                                                   */
/*********************************************************************************/
typedef struct TALPMetadataRecord {
    char *time_of_creation; // ISO 8601 string 
    char *dlb_version; // DLB version used
} talp_metadata_record_t;
static talp_metadata_record_t metadata_record;

void talp_output_record_metadata() {
    /* Initialize structure */
    time_t now = time(NULL);
    metadata_record = (const talp_metadata_record_t) {
        .time_of_creation = get_iso_8601_string(gmtime(&now)),
        .dlb_version = PACKAGE_VERSION
    };
}

static void metadata_to_json(FILE *out_file) {
    fprintf(out_file,
                    "  \"dlbVersion\": \"%s\",\n"
                    "  \"timestamp\": \"%s\",\n",
                metadata_record.dlb_version,metadata_record.time_of_creation);
}

static void metadata_to_xml(FILE *out_file) {

    fprintf(out_file,
            "  <dlbVersion>%s</dlbVersion>\n"
            "  <timestamp>%s</timestamp>\n",
            metadata_record.dlb_version,
            metadata_record.time_of_creation);
}

static void metadata_to_txt(FILE *out_file) {
  
    fprintf(out_file,
            "################# TALP Metadata ##################\n"
            "### DLB Version:                   %s\n"
            "### Timestamp:                     %s\n",
            metadata_record.dlb_version,
            metadata_record.time_of_creation);
}

static void metadata_finalize(void) {
    free(metadata_record.time_of_creation);
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

    talp_output_record_metadata();
    
    if (output_file == NULL) {
        /* No output file, just print all records */
        pop_metrics_print();
        pop_raw_print();
        node_print();
        process_print();
    } else {
        /* Do not open file if process has no data */
        if (pop_metrics_num_records + pop_raw_num_records == 0
                && node_list_head == NULL
                && monitor_list_head == NULL) return;

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

        /* Specific case where output file needs to be splitted */
        if (extension == EXT_CSV
                && !!(pop_metrics_num_records + pop_raw_num_records > 0)
                    + !!(node_list_head != NULL)
                    + !!(monitor_list_head != NULL) > 1) {

            /* Length without extension */
            int filename_useful_len = ext - output_file;

            /* POP */
            if (pop_metrics_num_records + pop_raw_num_records > 0) {
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
                    pop_to_csv(pop_file, append_to_csv);
                    fclose(pop_file);
                }
            }

            /* Node */
            if (node_list_head != NULL) {
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
            if (monitor_list_head != NULL) {
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
                        metadata_to_json(out_file);
                        pop_metrics_to_json(out_file);
                        pop_raw_to_json(out_file);
                        node_to_json(out_file);
                        process_to_json(out_file);
                        json_footer(out_file);
                        break;
                    case EXT_XML:
                        xml_header(out_file);
                        metadata_to_xml(out_file);
                        pop_metrics_to_xml(out_file);
                        pop_raw_to_xml(out_file);
                        node_to_xml(out_file);
                        process_to_xml(out_file);
                        xml_footer(out_file);
                        break;
                    case EXT_CSV:
                        pop_to_csv(out_file, append_to_csv);
                        node_to_csv(out_file, append_to_csv);
                        process_to_csv(out_file, append_to_csv);
                        break;
                    case EXT_TXT:
                        metadata_to_txt(out_file);
                        pop_metrics_to_txt(out_file);
                        pop_raw_to_txt(out_file);
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
    metadata_finalize();
    pop_metrics_finalize();
    pop_raw_finalize();
    node_finalize();
    process_finalize();
}
