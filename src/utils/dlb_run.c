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

/*! \page dlb_run Execute application with DLB pre-initialized.
 *  \section synopsis SYNOPSIS
 *      <B>dlb_run</B> [--verbose] <B>\<application\></B>
 *  \section description DESCRIPTION
 *      Execute \underline{application} in a pre-initialized DLB environment.
 *
 *      This is optional on most occasions, since DLB can be initialized at any
 *      time during the execution, but it is mandatory if using OMPT support.
 *      In that case, DLB needs to register the application and obtain the
 *      process CPU affinity mask before the OpenMP runtime creates all the
 *      threads and changes the CPU affinity mask of each one.
 *  \section options OPTIONS
 *      <DL>
 *          <DT>--verbose</DT>
 *          <DD>Enable verbose mode. Print the command to be executed and
 *          its return code upon finalization.</DD>
 *      </DL>
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (pm-tools@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb "dlb"(1), \ref dlb_shm "dlb_shm"(1),
 *      \ref dlb_taskset "dlb_taskset"(1)
 */

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
#include <signal.h>
#include <string.h>
#include <stdarg.h>

static bool verbose = false;

static void dlb_run_info_(FILE *out, const char *fmt, va_list list) {
    fprintf(out, "[dlb_run]: ");
    vfprintf(out, fmt, list);
}

static void dlb_run_info(FILE *out, const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    dlb_run_info_(out, fmt, list);
    va_end(list);
}

static void dlb_check(int error, const char *func) {
    if (error) {
        dlb_run_info(stderr, "Operation %s did not succeed\n", func);
        dlb_run_info(stderr, "Return code %d (%s)\n", error, DLB_Strerror(error));
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

/* Wait for child process to finalize, print status if needed, and finalize DLB */
static void wait_child(void) {
    int status;
    wait(&status);
    if (WIFEXITED(status) && verbose){
        dlb_run_info(stdout, "Child ended normally, return code: %d\n", WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)){
        int signal = WTERMSIG(status);
        dlb_run_info(stdout, "Child was terminated with signal %d (%s)\n",
                signal, strsignal(signal));
    }

    int error = DLB_Finalize();
    if (error != DLB_SUCCESS && error != DLB_ERR_NOPROC) {
        // DLB_ERR_NOPROC must be ignored here
        dlb_check(error, __FUNCTION__);
    }
}

/* This signal handler is called after X seconds while trying to correctly
 * finalize after a signal. In case of timeout, forcefully finalize */
static void hard_sighandler(int signum) {
    dlb_run_info(stderr,
            "Could not finalize child or DLB after a finalization signal\n");
    dlb_run_info(stderr,
            "If needed, please, execute dlb_shm -d to manually clean the DLB shared memories\n");
    exit(EXIT_FAILURE);
}

/* This signal handler is called if dlb_run is signaled with some common
 * termination signals */
static void soft_sighandler(int signum) {
    enum { ALARM_SECONDS = 5 };
    if (verbose) {
        dlb_run_info(stdout, "Catched signal %d, trying to finalize"
                " within %d seconds.\n", signum, ALARM_SECONDS);
    }

    /* Set up alarm signal to forcefully finalize if child or DLB_Finalize
     * cannot finalize in their own */
    struct sigaction sa = { .sa_handler = &hard_sighandler };
    sigaction(SIGALRM, &sa, NULL);
    alarm(ALARM_SECONDS);

    /* (try to) wait for child process */
    wait_child();

    exit(EXIT_FAILURE);
}

static void __attribute__((__noreturn__)) execute(char **argv) {
    /* PreInit from current process */
    cpu_set_t mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &mask);
    int error = DLB_PreInit(&mask, NULL);
    dlb_check(error, __FUNCTION__);

    if (verbose) {
        dlb_run_info(stdout, "Parent process (pid: %d) executing child %s\n", getpid(), argv[0]);
        fflush(stdout);
    }

    /* Set up signal handler to properly finalize DLB */
    struct sigaction sa = { .sa_handler = &soft_sighandler };
    sigfillset(&sa.sa_mask);
    sigdelset(&sa.sa_mask, SIGALRM);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Fork-exec */
    pid_t pid = fork();
    if (pid < 0) {
        dlb_run_info(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        if (verbose) {
            dlb_run_info(stdout, "Child process (pid: %d) executing...\n", getpid());
        }
        execvp(argv[0], argv);
        dlb_run_info(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    wait_child();

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
