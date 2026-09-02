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
#include "support/mask_utils.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void cat_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return;
    }

    fprintf(stderr, "--- File %s\n", filename);

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        // print to stderr so that the output is not cluttered with dlb messages
        if (fwrite(buffer, 1, n, stderr) != n) {
            perror("fwrite");
            break;
        }
    }

    if (ferror(f)) {
        perror("fread");
    }

    fprintf(stderr, "---\n");
    fclose(f);
}

static void write_to_file(const char *filename, const char *contents) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return;
    }

    size_t len = strlen(contents);
    fwrite(contents, 1, len, f);
    fclose(f);
}

static void record_pop_metrics(void) {

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
        .min_mpi_normd_proc           = 1000,
        .min_mpi_normd_node           = 1000,
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
}

static void record_process_metrics(void) {

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

static void record_metrics(void) {

    record_pop_metrics();
    record_process_metrics();
}

static void record_wrong_metrics(void) {

    /* Initialize structure */
    dlb_pop_metrics_t metrics = {
        .name                         = "Region 1",
        .num_cpus                     = 1,
        .num_mpi_ranks                = 0,
        .num_nodes                    = 1,
        .avg_cpus                     = 1.0f,
        .cycles                       = -100,
        .instructions                 = -200,
        .parallel_efficiency          = 1.24f,
        .mpi_parallel_efficiency      = 1.25f,
        .mpi_communication_efficiency = -1.0f,
        .mpi_load_balance             = 1.25f,
        .mpi_load_balance_in          = 1.5f,
        .mpi_load_balance_out         = 1.5f,
        .omp_parallel_efficiency      = 1.85f,
        .omp_load_balance             = 1.95f,
        .omp_scheduling_efficiency    = 1.95f,
        .omp_serialization_efficiency = 1.95f,
    };

    talp_output_record_pop_metrics(&metrics);

    const process_record_t process_record = {
        .rank = 0,
        .pid = 111,
        .hostname = "hostname",
        .cpuset = "[0-3]",
        .cpuset_quoted = "\"0-3\"",
        .monitor = {
            .cycles = -1,
            .instructions = -2,
        },
    };

    talp_output_record_process("Region 1", &process_record, 1);
}

int main(int argc, char *argv[]) {

    bool no_partial_output = false;
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    pid_t pid = getpid();

    /* Create temporary directory for TALP output files */
    const char *tmpdir_env = getenv("TMPDIR");
    asprintf(&tmpdir_template, "%s/dlb_test.XXXXXX", tmpdir_env ? tmpdir_env : "/tmp");
    tmpdir = mkdtemp(tmpdir_template);
    printf("%s\n", tmpdir);

    /* JSON */
    {
        char *json_filename;
        asprintf(&json_filename, "%s/talp.json", tmpdir);

        /* Base */
        record_metrics();
        talp_output_finalize(json_filename, no_partial_output);
        assert( access(json_filename, F_OK) == 0);
        cat_file(json_filename);
        int num_lines_in_json = count_lines(json_filename);

        /* Test that processInfo adds 4 lines */
        record_metrics();
        talp_output_record_process_info();
        talp_output_finalize(json_filename, no_partial_output);
        assert( access(json_filename, F_OK) == 0);
        cat_file(json_filename);
        int num_lines_in_json_w_process_info = count_lines(json_filename);
        assert( num_lines_in_json + 4 == num_lines_in_json_w_process_info );

        free(json_filename);
    }

    /* JSON with template*/
    {
        char *json_template;
        asprintf(&json_template, "%s/talp_%%h_%%p.json", tmpdir);

        char *expected_filename;
        asprintf(&expected_filename, "%s/talp_%s_%d.json", tmpdir, hostname, pid);

        record_metrics();
        talp_output_finalize(json_template, no_partial_output);
        assert( access(expected_filename, F_OK) == 0);
        cat_file(expected_filename);

        free(json_template);
        free(expected_filename);
    }

    /* JSON with partial output */
    {
        bool partial_output = true;

        char *json_filename;
        asprintf(&json_filename, "%s/talp.json", tmpdir);

        char *expected_filename;
        asprintf(&expected_filename, "%s/talp_%s_%d.partial.json", tmpdir, hostname, pid);

        record_metrics();
        talp_output_finalize(json_filename, partial_output);
        assert( access(expected_filename, F_OK) == 0 );
        cat_file(expected_filename);

        free(json_filename);
        free(expected_filename);
    }

    /* CSV multiple files */
    {
        char *csv_filename;
        asprintf(&csv_filename, "%s/talp.csv", tmpdir);
        unlink(csv_filename);

        char *expected_csv1, *expected_csv2;
        asprintf(&expected_csv1, "%s/talp-pop.csv", tmpdir);
        asprintf(&expected_csv2, "%s/talp-process.csv", tmpdir);

        record_metrics();
        talp_output_finalize(csv_filename, no_partial_output);
        assert( access(csv_filename, F_OK) != 0 );
        assert( access(expected_csv1, F_OK) == 0 );
        assert( access(expected_csv2, F_OK) == 0 );

        cat_file(expected_csv1);
        cat_file(expected_csv2);

        free(csv_filename);
        free(expected_csv1);
        free(expected_csv2);
    }

    /* CSV single file + append */
    {
        char *csv_filename;
        asprintf(&csv_filename, "%s/talp.csv", tmpdir);

        // first record
        record_pop_metrics();
        talp_output_finalize(csv_filename, no_partial_output);
        assert( access(csv_filename, F_OK) == 0 );
        assert( count_lines(csv_filename) == 2 );

        // second record
        record_pop_metrics();
        talp_output_finalize(csv_filename, no_partial_output);
        assert( access(csv_filename, F_OK) == 0 );
        assert( count_lines(csv_filename) == 3 );

        cat_file(csv_filename);
        free(csv_filename);
    }

    /* CSV with an old schema */
    {
        char *existing_csv;
        asprintf(&existing_csv, "%s/talp.csv", tmpdir);
        write_to_file(existing_csv, "old_schema,Header1,Header2\n1,2,3\n");

        char *expected_bak_csv;
        asprintf(&expected_bak_csv, "%s/talp_bak01.csv", tmpdir);

        // first record
        record_pop_metrics();
        talp_output_finalize(existing_csv, no_partial_output);
        assert( access(existing_csv, F_OK) == 0 );
        assert( count_lines(existing_csv) == 2 );
        assert( access(expected_bak_csv, F_OK) == 0 );
        assert( count_lines(expected_bak_csv) == 2 );

        // second record
        record_pop_metrics();
        talp_output_finalize(existing_csv, no_partial_output);
        assert( access(existing_csv, F_OK) == 0 );
        assert( count_lines(existing_csv) == 3 );
        assert( access(expected_bak_csv, F_OK) == 0 );
        assert( count_lines(expected_bak_csv) == 2 );

        cat_file(expected_bak_csv);
        cat_file(existing_csv);

        free(expected_bak_csv);
        free(existing_csv);
    }

    /* TXT */
    {
        char *txt_filename;
        asprintf(&txt_filename, "%s/talp.txt", tmpdir);

        record_metrics();
        talp_output_finalize(txt_filename, no_partial_output);
        assert( access(txt_filename, F_OK) == 0 );

        free(txt_filename);
    }

    /* No file */
    {
        record_metrics();
        talp_output_finalize(NULL, no_partial_output);
    }

    /* Output to /dev/null */
    {
        record_metrics();
        talp_output_finalize("/dev/null", no_partial_output);
    }

    /* Output to a directory that it does not exist */
    {
        char *subdir_filename;
        asprintf(&subdir_filename, "%s/subdir/talp.json", tmpdir);

        record_metrics();
        talp_output_finalize(subdir_filename, no_partial_output);
        assert( access(subdir_filename, F_OK) == 0 );
        cat_file(subdir_filename);

        free(subdir_filename);
    }

    /* Output to an existing file that cannot be written to
     * (e.g., a directory or a file without writing permissions) */
    {
        record_metrics();
        fprintf(stdout, "--- Fallback metrics in txt format:\n");
        talp_output_finalize(tmpdir, no_partial_output);
        fflush(stdout);
    }

    /* Output to a directory that cannot be created */
    {
        char *wrong_subdir;
        asprintf(&wrong_subdir, "/subdir/talp.json");

        record_metrics();
        fprintf(stdout, "--- Fallback metrics in json format:\n");
        talp_output_finalize(wrong_subdir, no_partial_output);
        fflush(stdout);

        free(wrong_subdir);
    }

    /* Report single region */
    {
        fprintf(stdout, "--- Single region:\n");
        dlb_monitor_t monitor = {
            .name                    = "Region 1",
            .num_cpus                = 1,
            .cycles                  = 1e9,
            .instructions            = 2e9,
            .num_measurements        = 1,
            .num_mpi_calls           = 5,
            .num_omp_parallels       = 2,
            .num_omp_tasks           = 7,
            .num_gpu_runtime_calls   = 42,
            .start_time              = 1e9,
            .stop_time               = 2e9,
            .elapsed_time            = 1e9,
            .useful_time             = 4e8,
            .mpi_time                = 2e8,
            .omp_load_imbalance_time = 1e8,
            .omp_scheduling_time     = 1e8,
            .omp_serialization_time  = 1e8,
            .gpu_runtime_time        = 1e8,
            .gpu_useful_time         = 2e8,
            .gpu_communication_time  = 2e8,
            ._data                   = calloc(1, sizeof(monitor_data_t)),
        };

        talp_flags_t flags = {
            .have_mpi = true,
            .have_openmp = true,
            .have_gpu = true,
            .have_hwc = true,
        };
        talp_output_print_monitoring_region(&monitor, flags);
        free(monitor._data);
        fflush(stdout);
    }

    /* Sanity checks */
    {
        fprintf(stdout, "--- Wrong metrics:\n");
        record_wrong_metrics();
        talp_output_finalize(NULL, no_partial_output);
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}
