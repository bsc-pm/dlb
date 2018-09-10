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

#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/wait.h>

static bool verbose = false;

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
                "\t%1$s <application>\n"
                "\n"
                ), program);

    fputs("Run application with DLB support.\n\n", out);

    fputs((
                "Options:\n"
                "  -h, --help               print this help\n"
                "      --verbose            enable verbose mode\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) execute(char **argv) {
    /* PreInit from current process */
    cpu_set_t mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &mask);
    int error = DLB_PreInit(&mask, NULL);
    dlb_check(error, __FUNCTION__);

    if (verbose) {
        fprintf(stdout, "Executing child %s\n", argv[0]);
        fflush(stdout);
    }

    /* Fork-exec */
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Finalize from current process */
    int status;
    wait(&status);
    if (verbose) {
        if (WIFEXITED(status)){
            fprintf(stdout, "Child ended normally, return code: %d\n", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status)){
            fprintf(stdout, "Child was signaled with %d\n", WTERMSIG(status));
        }
    }

    error = DLB_Finalize();
    if (error != DLB_SUCCESS && error != DLB_ERR_NOPROC) {
        // DLB_ERR_NOPROC must be ignored here
        dlb_check(error, __FUNCTION__);
    }
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {

    /* Long options that have no corresponding short option */
    enum {
        VERBOSE_OPTION = CHAR_MAX + 1
    };

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"help",     no_argument,       NULL, 'h'},
        {"version",  no_argument,       NULL, 'v'},
        {"verbose",  no_argument,       NULL, VERBOSE_OPTION},
        {0,          0,                 NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, "+hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0], stdout);
                break;
            case 'v':
                version();
                break;
            case VERBOSE_OPTION:
                verbose = true;
                break;
            default:
                usage(argv[0], stderr);
                break;
        }
    }

    // Actions
    if (argc > optind) {
        argv += optind;
        execute(argv);
    }
    else {
        usage(argv[0], stderr);
    }

    return EXIT_SUCCESS;
}
