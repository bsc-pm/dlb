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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "apis/dlb_talp.h"
#include "support/talp_output.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

static char *tmpdir_template = NULL;
static char *tmpdir = NULL;

static int remove_callback(const char *fpath, const struct stat *sb,
        int typeflag, struct FTW *ftwbuf) {
    return remove(fpath);
}

__attribute__((destructor))
static void delete_test_directory(void) {
    if (tmpdir != NULL) {
        nftw (tmpdir, remove_callback, 1, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
    }

    tmpdir = NULL;
    free(tmpdir_template);
    tmpdir_template = NULL;
}

static int count_lines(const char *filename) {
    int lines = 0;
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        int c;
        for (c = getc(file); c != EOF; c = getc(file)) {
            if (c == '\n') {
                ++lines;
            }
        }
        fclose(file);
    }
    return lines;
}
static void record_metrics(void) {
    /* Initialize structure */
    dlb_pop_metrics_t metrics = {
        .name                         = "Region 1",
        .num_cpus                     = 1,
        .num_mpi_ranks                = 0,
        .num_nodes                    = 1,
        .avg_cpus                     = 1.0f,
        .cycles                       = 1e9,
        .instructions                 = 2e9,
        .num_mpi_calls                = 0,
        .elapsed_time                 = 1000000000,
        .useful_time                  = 500000000,
        .mpi_time                     = 0,
        .omp_load_imbalance_time      = 100000000,
        .omp_scheduling_time          = 100000000,
        .omp_serialization_time       = 300000000,
        .useful_normd_app             = (double)1000000000 / 1,
        .mpi_normd_app                = 0,
        .max_useful_normd_proc        = 500000000,
        .max_useful_normd_node        = 500000000,
        .mpi_normd_of_max_useful      = 0,
        .parallel_efficiency          = 0.24f,
        .mpi_parallel_efficiency      = 0.25f,
        .mpi_communication_efficiency = 1.0f,
        .mpi_load_balance             = 0.25f,
        .mpi_load_balance_in          = 0.5f,
        .mpi_load_balance_out         = 0.5f,
        .omp_parallel_efficiency      = 0.85f,
        .omp_load_balance             = 0.95f,
        .omp_scheduling_efficiency    = 0.95f,
        .omp_serialization_efficiency = 0.95f,
    };


    talp_output_record_pop_metrics(&metrics);

    /* node_record_t contains a flexible array member and it needs to be
     * dynamically allocated */
    node_record_t *node_record = malloc(sizeof(node_record_t)
            + sizeof(process_in_node_record_t) * 2);
    *node_record = (const node_record_t) {
        .node_id = 0,
        .nelems = 2,
        .avg_useful_time = 100,
        .avg_mpi_time = 100,
        .max_useful_time = 200,
        .max_mpi_time = 200,
    };
    node_record->processes[0] = (const process_in_node_record_t) {
        .pid = 111, .mpi_time = 100, .useful_time = 200,
    };
    node_record->processes[1] = (const process_in_node_record_t) {
        .pid = 111, .mpi_time = 100, .useful_time = 200,
    };

    talp_output_record_node(node_record);

    const process_record_t process_record = {
        .rank = 0,
        .pid = 111,
        .hostname = "hostname",
        .cpuset = "[0-3]",
        .cpuset_quoted = "\"0-3\"",
        .monitor = {
            .num_measurements = 1,
            .elapsed_time = 100,
            .useful_time = 100,
            .mpi_time = 100,
        },
    };

    talp_output_record_process("Region 1", &process_record, 1);
}

int main(int argc, char *argv[]) {

    int error = 0;

    /* Create temporary directory for TALP output files */
    const char *tmpdir_env = getenv("TMPDIR");
    asprintf(&tmpdir_template, "%s/dlb_test.XXXXXX", tmpdir_env ? tmpdir_env : "/tmp");
    tmpdir = mkdtemp(tmpdir_template);
    printf("%s\n", tmpdir);

    /* JSON */
    char *json_filename;
    asprintf(&json_filename, "%s/talp.json", tmpdir);
    record_metrics();
    talp_output_finalize(json_filename);
    error += access(json_filename, F_OK);
    free(json_filename);

    /* XML */
    char *xml_filename;
    asprintf(&xml_filename, "%s/talp.xml", tmpdir);
    record_metrics();
    talp_output_finalize(xml_filename);
    error += access(xml_filename, F_OK);
    free(xml_filename);

    /* CSV */
    char *csv_filename, *csv1, *csv2, *csv3;
    asprintf(&csv_filename, "%s/talp.csv", tmpdir);
    dlb_pop_metrics_t metrics_1 = { .name = "Region 1" };
    talp_output_record_pop_metrics(&metrics_1);
    talp_output_finalize(csv_filename);
    error += access(csv_filename, F_OK);
    dlb_pop_metrics_t metrics_2 = { .name = "Region 2" };
    talp_output_record_pop_metrics(&metrics_2);
    talp_output_finalize(csv_filename);
    error += count_lines(csv_filename) - 3;  // test append, count_lines should return 3
    asprintf(&csv1, "%s/talp-pop.csv", tmpdir);
    asprintf(&csv2, "%s/talp-node.csv", tmpdir);
    asprintf(&csv3, "%s/talp-process.csv", tmpdir);
    record_metrics();
    talp_output_finalize(csv_filename);
    error += access(csv1, F_OK);
    error += access(csv2, F_OK);
    error += access(csv3, F_OK);
    free(csv_filename);
    free(csv1);
    free(csv2);
    free(csv3);

    /* TXT */
    char *txt_filename;
    asprintf(&txt_filename, "%s/talp.txt", tmpdir);
    record_metrics();
    talp_output_finalize(txt_filename);
    error += access(txt_filename, F_OK);
    free(txt_filename);

    /* No file */
    record_metrics();
    talp_output_finalize(NULL);

    return error;
}
