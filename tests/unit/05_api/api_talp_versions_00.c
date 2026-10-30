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

/* No testinfo for bets. This test needs the abi directory and we don't have an
 * easy way of passing it. Until we implement it, this test runs only with Meson. */

#include "tmpdir.h"

#include "apis/dlb_talp.h"
#include "talp/talp_output.h"
#include "talp/talp_types.h"

#include <ftw.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Check versions of TALP public structs


// NOTE: uses local variables `buf` and `off` for convenience.
#define APPEND(...) \
    do { off += (size_t)snprintf(buf + off, sizeof(buf) - off, __VA_ARGS__); } while (0)

enum { GOLDEN_BUF_SIZE = 4096 };


static char* slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);

    char *s = malloc((size_t)n + 1);
    if (s) {
        size_t i = fread(s, 1, (size_t)n, f);
        s[i] = '\0';
    }
    fclose(f);

    return s;
}

/* Check that `text` is equal to `golden`, otherwise conditionally updates `path`:
 * - If DLB_ABI_UPDATE is '1' or 'y', add new golden files if needed but does not overwrite.
 * - If DLB_ABI_UPDATE is 'overwrite', write files if they differ.
 */
static int check_golden_impl(const char *path, const char *golden, const char *what, const char *text)
{
    bool exists = golden != NULL;
    bool same = exists && strcmp(golden, text) == 0;

    if (same) {
        return 0;
    }

    const char *update_env = getenv("DLB_ABI_UPDATE");
    bool update = update_env && (update_env[0] == '1' || update_env[0] == 'y');
    bool overwrite = update_env && !update && strcmp(update_env, "overwrite") == 0;

    if (exists && !overwrite) {
        fprintf(stderr,
                "%s: %s already exists and differs.\n"
                "If its version was already released, bump the version instead.\n"
                "If this version is still unreleased, rerun with DLB_ABI_UPDATE=overwrite.\n",
                what, path);
        return 1;
    }

    if (update || overwrite) {
        FILE *f = fopen(path, "wb");
        if (!f || fputs(text, f) == EOF) { perror(path); return 1; }
        fclose(f);
        printf("wrote %s\n", path);
        return 0;
    }

    if (golden) {
        fprintf(stderr, "%s output changed without bumping its version\n"
                        "--- expected (%s)\n"
                        "%s\n"
                        "--- current\n"
                        "%s\n",
                        what, path, golden, text);
    } else {
        fprintf(stderr, "%s: no snapshot. Run with DLB_ABI_UPDATE=1 and commit %s\n", what, path);
    }

    return 1;
}

static int check_golden(const char *dir, const char *type, int version,
        const char *arch, const char *ext, const char *text)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path),
            "%s/%s.v%d%s%s.%s",
            dir, type, version,
            arch ? "." : "",
            arch ? arch : "",
            ext);

    char *golden = slurp(path);

    int ret = check_golden_impl(path, golden, type, text);

    free(golden);

    return ret;
}

static int check_dlb_monitor_t(const char *dir, const char *arch) {

    char buf[GOLDEN_BUF_SIZE];
    size_t off = 0;

    APPEND("version %d\nsizeof %zu\nalignof %zu\n",
            (int)DLB_MONITOR_T_VERSION, sizeof(dlb_monitor_t), _Alignof(dlb_monitor_t));

    #define DUMP(name, c_type, ...) \
        APPEND("field %s %s offset %zu size %zu\n", #name, #c_type, \
                offsetof(dlb_monitor_t, name), sizeof(((dlb_monitor_t *)0)->name));
    FOR_DLB_MONITOR_FIELDS(DUMP)
    #undef DUMP

    return check_golden(dir, "dlb_monitor_t", DLB_MONITOR_T_VERSION, arch, "layout", buf);
}

static int check_dlb_pop_metrics_t(const char *dir, const char *arch) {

    char buf[GOLDEN_BUF_SIZE];
    size_t off = 0;

    APPEND("version %d\nsizeof %zu\nalignof %zu\n",
            (int)DLB_POP_METRICS_T_VERSION, sizeof(dlb_pop_metrics_t),
            _Alignof(dlb_pop_metrics_t));

    #define DUMP(name, c_type, ...) \
        APPEND("field %s %s offset %zu size %zu\n", #name, #c_type, \
                offsetof(dlb_pop_metrics_t, name), sizeof(((dlb_pop_metrics_t *)0)->name));
    FOR_DLB_POP_METRICS_FIELDS(DUMP, DUMP)
    #undef DUMP

    return check_golden(dir, "dlb_pop_metrics_t", DLB_POP_METRICS_T_VERSION, arch, "layout", buf);
}

static int check_dlb_node_metrics_t(const char *dir, const char *arch) {

    char buf[GOLDEN_BUF_SIZE];
    size_t off = 0;

    APPEND("version %d\nsizeof %zu\nalignof %zu\n",
            (int)DLB_NODE_METRICS_T_VERSION, sizeof(dlb_node_metrics_t),
            _Alignof(dlb_node_metrics_t));

    #define DUMP(name, c_type, ...) \
        APPEND("field %s %s offset %zu size %zu\n", #name, #c_type, \
                offsetof(dlb_node_metrics_t, name), sizeof(((dlb_node_metrics_t *)0)->name));
    FOR_DLB_NODE_METRICS_FIELDS(DUMP)
    #undef DUMP

    return check_golden(dir, "dlb_node_metrics_t", DLB_NODE_METRICS_T_VERSION, arch, "layout", buf);
}

static int check_json_output(const char *dir) {

    const dlb_pop_metrics_t metrics = { .name = "<Name>" };
    talp_output_record_pop_metrics(&metrics);

    const process_record_t process_record = {0};
    talp_output_record_process("<Name>", &process_record, 1);

    talp_output_testing_use_fixed_metadata(true);

    char json_path[PATH_MAX];
    tmpdir_path(json_path, sizeof(json_path), "talp.json");

    bool no_partial_output = false;
    talp_output_finalize(json_path, no_partial_output);

    char *text = slurp(json_path);

    int rc = check_golden(dir, "talp_output", TALP_OUTPUT_JSON_SCHEMA_VERSION, NULL, "json", text);

    free(text);

    return rc;
}

int main(int argc, char **argv) {

    /* For now we're only interested for CI reasons, we can skip other architectures. */
#if !defined(__x86_64__) || !defined(__linux__)
    // skip RC convention in Meson
    return 77;
#endif

#define ABI_ARCH "x86_64"

    const char *dir;

    if (argc > 1) {
        /* Meson passes dir by argument */
        dir = argv[1];
    } else if ((dir = getenv("DLB_ABI_DIR")) != NULL) {
        /* We could use environment variables also, but lit starts with a clean
         * environment. This is left here in case we use it in the future. */
    } else {
        dir = "tests/abi";
    }

    int err = 0;

    err += check_dlb_monitor_t(dir, ABI_ARCH);
    err += check_dlb_pop_metrics_t(dir, ABI_ARCH);
    err += check_dlb_node_metrics_t(dir, ABI_ARCH);

    err += check_json_output(dir);

    return err;
}
