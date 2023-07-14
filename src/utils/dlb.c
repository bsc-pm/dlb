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

/*! \page dlb DLB utility command.
 *  \section synopsis SYNOPSIS
 *      <B>dlb</B> {--affinity | --help [--help] | --version}
 *  \section description DESCRIPTION
 *      Utility command to display all DLB options, print DLB version, or check
 *      process affinity.
 *
 *      <DL>
 *          <DT>-a, --affinity</DT>
 *          <DD>Print process affinity.</DD>
 *
 *          <DT>-h, --help</DT>
 *          <DD>Print DLB variables and current value. Use the option twice
 *          (e.g., -hh) for extended info.</DD>
 *
 *          <DT>-v, --version</DT>
 *          <DD>Print version info.</DD>
 *      </DL>
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (pm-tools@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb_run "dlb_run"(1), \ref dlb_shm "dlb_shm"(1),
 *      \ref dlb_taskset "dlb_taskset"(1)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "apis/dlb.h"
#include "support/mask_utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>


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
                "\t%1$s --affinity\n"
                "\t%1$s --help\n"
                "\t%1$s --version\n"
                "\n"
                ), program);

    fputs("DLB binary.\n\n", out);

    fputs((
                "Options:\n"
                "  -a, --affinity           print process affinity\n"
                "  -h, --help               print DLB variables and current value\n"
                "                           (twice `-hh` for extended)\n"
                "  -v, --version            print version info\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) print_variables(int print_extended) {
    int error = DLB_Init(0, NULL, NULL);
    dlb_check(error, __FUNCTION__);
    DLB_PrintVariables(print_extended);
    if (!print_extended) {
        fprintf(stdout, "Run dlb -hh for extended help\n");
    }
    error = DLB_Finalize();
    dlb_check(error, __FUNCTION__);
    exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) print_affinity(void) {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);

    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    printf("DLB[%s:%d]: Process affinity: %s\n",
            hostname, getpid(), mu_to_str(&process_mask));

    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {

    bool do_affinity = false;
    bool do_print = false;
    bool print_extended = false;

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"affinity",    no_argument,    NULL, 'a'},
        {"help",        no_argument,    NULL, 'h'},
        {"help-extra",  no_argument,    NULL, 'x'}, /* kept for compatibility */
        {"version",     no_argument,    NULL, 'v'},
        {0,             0,              NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, "ahxv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a':
                do_affinity = true;
                break;
            case 'h':
                if (do_print) {
                    print_extended = true;
                }
                do_print = true;
                break;
            case 'x':
                do_print = true;
                print_extended = true;
                break;
            case 'v':
                version();
                break;
            default:
                usage(argv[0], stderr);
        }
    }

    if (do_print) {
        print_variables(print_extended);
    } else if (do_affinity) {
        print_affinity();
    } else {
        usage(argv[0], stderr);
    }

    return EXIT_SUCCESS;
}
