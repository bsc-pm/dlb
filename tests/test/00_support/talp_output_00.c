/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

static void record_metrics(void) {

    talp_output_record_pop_metrics("Region 1", 100, 0.89f, 0.99f, 0.9f, 0.9f, 1.0f);

    talp_output_record_pop_raw("Region 1", 8, 1, 3, 100, 90, 500, 500, 42);

    process_in_node_record_t process[] = {
        {.pid = 111, .mpi_time = 100, .useful_time = 200 },
        {.pid = 111, .mpi_time = 100, .useful_time = 200 }
    };
    talp_output_record_node(0, 2, 100, 100, 200, 200, process);

    talp_output_record_process("Region 1", 0, 111, 1, "hostname",
            "[0-3]", "\"0-3\"", 100, 100, 100, 100);
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
    error += access(json_filename, F_OK );
    free(json_filename);

    /* XML */
    char *xml_filename;
    asprintf(&xml_filename, "%s/talp.xml", tmpdir);
    record_metrics();
    talp_output_finalize(xml_filename);
    error += access(xml_filename, F_OK );
    free(xml_filename);

    /* CSV */
    char *csv_filename, *csv1, *csv2, *csv3;
    asprintf(&csv_filename, "%s/talp.csv", tmpdir);
    talp_output_record_pop_raw("Region 1", 8, 1, 3, 100, 90, 500, 500, 42);
    talp_output_finalize(csv_filename);
    error += access(csv_filename, F_OK );
    asprintf(&csv1, "%s/talp-pop.csv", tmpdir);
    asprintf(&csv2, "%s/talp-node.csv", tmpdir);
    asprintf(&csv3, "%s/talp-process.csv", tmpdir);
    record_metrics();
    talp_output_finalize(csv_filename);
    error += access(csv1, F_OK );
    error += access(csv2, F_OK );
    error += access(csv3, F_OK );
    free(csv_filename);
    free(csv1);
    free(csv2);
    free(csv3);

    /* TXT */
    char *txt_filename;
    asprintf(&txt_filename, "%s/talp.txt", tmpdir);
    record_metrics();
    talp_output_finalize(txt_filename);
    error += access(txt_filename, F_OK );
    free(txt_filename);

    /* No file */
    record_metrics();
    talp_output_finalize(NULL);

    return error;
}
