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

#include "support/talp_output.h"

#include "LB_core/DLB_talp.h"
#include "support/debug.h"
#include "support/mytime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*********************************************************************************/
/*    POP Metrics                                                                */
/*********************************************************************************/
typedef struct POPMetricsRecord {
    char *name;
    int64_t elapsed_time;
    float parallel_efficiency;
    float communication_efficiency;
    float lb;
    float lb_in;
    float lb_out;
} pop_metrics_record_t;
static int pop_metrics_num_records = 0;
static pop_metrics_record_t *pop_metrics = NULL;

void talp_output_record_pop_metrics(const char *name, int64_t elapsed_time,
        float parallel_efficiency, float communication_efficiency,
        float lb, float lb_in, float lb_out) {

    /* Realloc array */
    ++pop_metrics_num_records;
    void *ptr = realloc(pop_metrics,
            pop_metrics_num_records*sizeof(pop_metrics_record_t));
    fatal_cond(!ptr, "realloc failed");
    pop_metrics = ptr;

    /* Allocate monitor name */
    char *allocated_name = malloc(MONITOR_MAX_KEY_LEN*sizeof(char));
    snprintf(allocated_name, MONITOR_MAX_KEY_LEN, "%s", name);

    /* Initialize structure */
    pop_metrics[pop_metrics_num_records-1] = (const pop_metrics_record_t) {
        .name = allocated_name,
        .elapsed_time = elapsed_time,
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
            info("### Parallel efficiency :       %1.2f", record->parallel_efficiency);
            info("###   - Communication eff. :    %1.2f", record->communication_efficiency);
            info("###   - Load Balance :          %1.2f", record->lb);
            info("###       - LB_in :             %1.2f", record->lb_in);
            info("###       - LB_out:             %1.2f", record->lb_out);
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
                    "      \"parallelEfficiency\": %.2f,\n"
                    "      \"communicationEfficiency\": %.2f,\n"
                    "      \"loadBalance\": %.2f,\n"
                    "      \"lbIn\": %.2f,\n"
                    "      \"lbOut\": %.2f\n"
                    "    }%s\n",
                    record->name,
                    record->elapsed_time,
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
                "    <parallelEfficiency>%.2f</parallelEfficiency>\n"
                "    <communicationEfficiency>%.2f</communicationEfficiency>\n"
                "    <loadBalance>%.2f</loadBalance>\n"
                "    <lbIn>%.2f</lbIn>\n"
                "    <lbOut>%.2f</lbOut>\n"
                "  </popMetrics>\n",
                record->name,
                record->elapsed_time,
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
                    "### Parallel efficiency :       %1.2f\n"
                    "###   - Communication eff. :    %1.2f\n"
                    "###   - Load Balance :          %1.2f\n"
                    "###       - LB_in :             %1.2f\n"
                    "###       - LB_out:             %1.2f\n",
                    record->name,
                    elapsed_time_str,
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
    int64_t elapsed_time;
    int64_t elapsed_useful;
    int64_t app_sum_useful;
    int64_t node_sum_useful;
} pop_raw_record_t;
static int pop_raw_num_records = 0;
static pop_raw_record_t *pop_raw = NULL;

void talp_output_record_pop_raw(const char *name, int P, int N, int64_t elapsed_time,
        int64_t elapsed_useful, int64_t app_sum_useful, int64_t node_sum_useful) {

    /* Realloc array */
    ++pop_raw_num_records;
    void *ptr = realloc(pop_raw, pop_raw_num_records*sizeof(pop_raw_record_t));
    fatal_cond(!ptr, "realloc failed");
    pop_raw = ptr;

    /* Allocate monitor name */
    char *allocated_name = malloc(MONITOR_MAX_KEY_LEN*sizeof(char));
    snprintf(allocated_name, MONITOR_MAX_KEY_LEN, "%s", name);

    /* Initialize structure */
    pop_raw[pop_raw_num_records-1] = (const pop_raw_record_t) {
        .name = allocated_name,
        .P = P,
        .N = N,
        .elapsed_time = elapsed_time,
        .elapsed_useful = elapsed_useful,
        .app_sum_useful = app_sum_useful,
        .node_sum_useful = node_sum_useful,
    };
}

static void pop_raw_print(void) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        pop_raw_record_t *record = &pop_raw[i];
        if (record->elapsed_time > 0) {
            info("######### Monitoring Region POP Raw Data #########");
            info("### Name:                       %s", record->name);
            info("### Number of CPUs:             %d", record->P);
            info("### Number of nodes:            %d", record->N);
            info("### Elapsed Time:               %"PRId64" ns", record->elapsed_time);
            info("### Elapsed Useful:             %"PRId64" ns", record->elapsed_useful);
            info("### Useful CPU Time (Total):    %"PRId64" ns", record->app_sum_useful);
            info("### Useful CPU Time (Node):     %"PRId64" ns", record->node_sum_useful);
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
                    "      \"elapsedTime\": %"PRId64",\n"
                    "      \"elapsedUseful\": %"PRId64",\n"
                    "      \"usefulCpuTotal\": %"PRId64",\n"
                    "      \"usefulCpuNode\": %"PRId64"\n"
                    "    }%s\n",
                    record->name,
                    record->P,
                    record->N,
                    record->elapsed_time,
                    record->elapsed_useful,
                    record->app_sum_useful,
                    record->node_sum_useful,
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
                "    <elapsedTime>%"PRId64"</elapsedTime>\n"
                "    <elapsedUseful>%"PRId64"</elapsedUseful>\n"
                "    <usefulCpuTotal>%"PRId64"</usefulCpuTotal>\n"
                "    <usefulCpuNode>%"PRId64"</usefulCpuNode>\n"
                "  </popRaw>\n",
                record->name,
                record->P,
                record->N,
                record->elapsed_time,
                record->elapsed_useful,
                record->app_sum_useful,
                record->node_sum_useful);
    }
}

static void pop_raw_to_txt(FILE *out_file) {
    int i;
    for (i=0; i<pop_raw_num_records; ++i) {
        pop_raw_record_t *record = &pop_raw[i];
        if (record->elapsed_time > 0) {
            fprintf(out_file,
                    "######### Monitoring Region POP Raw Data #########\n"
                    "### Name:                       %s\n"
                    "### Number of CPUs:             %d\n"
                    "### Number of nodes:            %d\n"
                    "### Elapsed Time:               %"PRId64" ns\n"
                    "### Elapsed Useful:             %"PRId64" ns\n"
                    "### Useful CPU Time (Total):    %"PRId64" ns\n"
                    "### Useful CPU Time (Node):     %"PRId64" ns\n",
                    record->name,
                    record->P,
                    record->N,
                    record->elapsed_time,
                    record->elapsed_useful,
                    record->app_sum_useful,
                    record->node_sum_useful);
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

static void pop_to_csv(FILE *out_file) {
    int num_records = max_int(pop_metrics_num_records, pop_raw_num_records);
    if (num_records == 0)
        return;

    /* Print header */
    fprintf(out_file, "Name%s%s\n",
            pop_metrics_num_records == 0 ? "" :
            ",parallelEfficiency,communicationEfficiency,loadBalance,lbIn,lbOut",
            pop_raw_num_records == 0 ? "" :
            ",numCpus,numNodes,elapsedTime,elapsedUseful,usefulCpuTotal,usefulCpuNode"
           );

    int i;
    for (i=0; i<num_records; ++i) {
        pop_metrics_record_t *metrics_record =
            i < pop_metrics_num_records ? &pop_metrics[i] : NULL;
        pop_raw_record_t *raw_record =
            i < pop_raw_num_records ? &pop_raw[i] : NULL;

        /* Assuming that if both records are not NULL, they are the same region */
        fprintf(out_file, "%s", metrics_record ? metrics_record->name : raw_record->name);

        if (metrics_record) {
            fprintf(out_file, ",%.2f,%.2f,%.2f,%.2f,%.2f",
                    metrics_record->parallel_efficiency,
                    metrics_record->communication_efficiency,
                    metrics_record->lb,
                    metrics_record->lb_in,
                    metrics_record->lb_out);
        }

        if (raw_record) {
            fprintf(out_file, ",%d,%d,%"PRId64",%"PRId64",%"PRId64",%"PRId64,
                    raw_record->P,
                    raw_record->N,
                    raw_record->elapsed_time,
                    raw_record->elapsed_useful,
                    raw_record->app_sum_useful,
                    raw_record->node_sum_useful);
        }

        fprintf(out_file, "\n");
    }
}


/*********************************************************************************/
/*    Node                                                                       */
/*********************************************************************************/
typedef struct ProcessInNodeRecord {
    pid_t pid;
    int64_t mpi_time;
    int64_t useful_time;
} process_in_node_record_t;
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
        void * process_info) {

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

static void node_to_csv(FILE *out_file) {
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

static void process_print(void) {
}

static void process_to_json(FILE *out_file) {
}

static void process_to_xml(FILE *out_file) {
}

static void process_to_csv(FILE *out_file) {
}

static void process_to_txt(FILE *out_file) {
}

static void process_finalize(void) {
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
    if (output_file == NULL) {
        /* No output file, just print all records */
        pop_metrics_print();
        pop_raw_print();
        node_print();
        process_print();
    } else {
        /* Do not open file if process has no data */
        if (pop_metrics_num_records + pop_raw_num_records == 0
                && node_list_head == NULL) return;

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
                FILE *pop_file = fopen(pop_filename, "w");
                if (pop_file == NULL) {
                    warning("Cannot open file %s: %s", pop_filename, strerror(errno));
                } else {
                    pop_to_csv(pop_file);
                    fclose(pop_file);
                }
            }

            /* Node */
            if (node_list_head != NULL) {
                const char *node_ext = "-node.csv";
                size_t node_file_len = filename_useful_len + strlen(node_ext) + 1;
                char *node_filename = malloc(sizeof(char)*node_file_len);
                sprintf(node_filename, "%.*s%s", filename_useful_len, output_file, node_ext);
                FILE *node_file = fopen(node_filename, "w");
                if (node_file == NULL) {
                    warning("Cannot open file %s: %s", node_filename, strerror(errno));
                } else {
                    node_to_csv(node_file);
                    fclose(node_file);
                }
            }

            /* Process */
            if (monitor_list_head != NULL) {
                const char *process_ext = "-process.csv";
                size_t process_file_len = filename_useful_len + strlen(process_ext) + 1;
                char *process_filename = malloc(sizeof(char)*process_file_len);
                sprintf(process_filename, "%.*s%s", filename_useful_len, output_file, process_ext);
                FILE *process_file = fopen(process_filename, "w");
                if (process_file == NULL) {
                    warning("Cannot open file %s: %s", process_filename, strerror(errno));
                } else {
                    process_to_csv(process_file);
                    fclose(process_file);
                }
            }
        }

        /* Write to file */
        else {
            /* Open file */
            FILE *out_file = fopen(output_file, "w");
            if (out_file == NULL) {
                warning("Cannot open file %s: %s", output_file, strerror(errno));
            } else {
                /* Write records to file */
                switch(extension) {
                    case EXT_JSON:
                        json_header(out_file);
                        pop_metrics_to_json(out_file);
                        pop_raw_to_json(out_file);
                        node_to_json(out_file);
                        process_to_json(out_file);
                        json_footer(out_file);
                        break;
                    case EXT_XML:
                        xml_header(out_file);
                        pop_metrics_to_xml(out_file);
                        pop_raw_to_xml(out_file);
                        node_to_xml(out_file);
                        process_to_xml(out_file);
                        xml_footer(out_file);
                        break;
                    case EXT_CSV:
                        pop_to_csv(out_file);
                        node_to_csv(out_file);
                        process_to_csv(out_file);
                        break;
                    case EXT_TXT:
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
    pop_metrics_finalize();
    pop_raw_finalize();
    node_finalize();
    process_finalize();
}
