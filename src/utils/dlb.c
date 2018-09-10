/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "apis/dlb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>


static void dlb_check(int error, const char *func) {
    if (error) {
        fprintf(stderr, "Operation %s did not succeed\n", func);
        fprintf(stderr, "Return code %d (%s)\n", error, DLB_Strerror(error));
        exit(EXIT_FAILURE);
    }
}

static void __attribute__((__noreturn__)) version(void) {
    fprintf(stdout, "%s %s (%s)\n", PACKAGE, VERSION, DLB_BUILD_VERSION);
    fprintf(stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS);
    exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
    fprintf(out, (
                "usage:\n"
                "\t%1$s --help\n"
                "\t%1$s --help-extra\n"
                "\t%1$s --version\n"
                "\n"
                ), program);

    fputs("DLB binary.\n\n", out);

    fputs((
                "Options:\n"
                "  -h, --help               print DLB variables and current value\n"
                "  -x, --help-extra         print DLB variables, including experimental\n"
                "  -v, --version            print version info\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

typedef enum print_level_e {
    PRINT_SIMPLE,
    PRINT_EXTRA
} print_level_t;

static void __attribute__((__noreturn__)) print_variables(print_level_t level) {
    int error = DLB_Init(0, NULL, NULL);
    dlb_check(error, __FUNCTION__);
    DLB_PrintVariables(0);
    if (level == PRINT_EXTRA) DLB_PrintVariables(1);
    error = DLB_Finalize();
    dlb_check(error, __FUNCTION__);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"help",        no_argument,    NULL, 'h'},
        {"help-extra",  no_argument,    NULL, 'x'},
        {"version",     no_argument,    NULL, 'v'},
        {0,             0,              NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, "hxv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_variables(PRINT_SIMPLE);
                break;
            case 'x':
                print_variables(PRINT_EXTRA);
                break;
            case 'v':
                version();
                break;
            default:
                usage(argv[0], stderr);
        }
    }

    usage(argv[0], stderr);

    return EXIT_SUCCESS;
}
