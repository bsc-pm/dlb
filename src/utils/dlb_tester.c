/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

/* This program will indefinitely run until some termination signal is captured */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <omp.h>

#include "apis/dlb.h"

enum { QUERY_INTERVAL_DEFAULT_MS = 1000 };

static volatile int keep_running = 1;

static void sighandler(int signum) {
    printf("Received signal %d, dying\n", signum);
    keep_running = 0;
}

/* Return timespec diff in ms */
static int timespec_diff(const struct timespec *init, const struct timespec *end) {
    return (int)(end->tv_sec - init->tv_sec) * 1000L +
        (int)(end->tv_nsec - init->tv_nsec) / 1000000L;
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB tester\n");
    fputs("Simulate a DLB program, print binding info at the specified interval.\n\n", out);

    fprintf(out, "usage: %s [OPTIONS]\n", program);
    fputs((
                "Options:\n"
                "  -b, --busy-wait          force 100%% CPU usage while waiting\n"
                "  -i, --interval <ms>      amount of milliseconds between each update\n"
                "  -h, --help               print this help\n"
                "\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    /* Register new actions for SIGINT and SIGTERM */
    struct sigaction new_action;
    new_action.sa_handler = sighandler;
    new_action.sa_flags = 0;
    sigemptyset(&new_action.sa_mask);

    if (sigaction(SIGINT, &new_action, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
    }
    if (sigaction(SIGTERM, &new_action, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
    }

    /* Parse options */
    bool busy_wait = false;
    int query_interval_ms = QUERY_INTERVAL_DEFAULT_MS;
    int opt;
    extern char *optarg;
    struct option long_options[] = {
        {"busy-wait", no_argument,       NULL, 'b'},
        {"interval",  required_argument, NULL, 'i'},
        {"help",      no_argument,       NULL, 'h'},
        {0,           0,                 NULL, 0 }
    };
    while ((opt = getopt_long(argc, argv, "bi:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'b':
                busy_wait = true;
                break;
            case 'i':
                query_interval_ms = strtol(optarg, NULL, 0);
                if (!query_interval_ms)
                    query_interval_ms = QUERY_INTERVAL_DEFAULT_MS;
                break;
            case 'h':
                usage(argv[0], stdout);
                break;
            default:
                usage(argv[0], stderr);
                break;
        }
    }
    printf("Query interval: %d ms\n", query_interval_ms);

    /* Initialize DLB */
    cpu_set_t mask;
    sched_getaffinity(0, sizeof(mask), &mask);
    DLB_Init(0, &mask, "--drom");
    int system_size = sysconf(_SC_NPROCESSORS_ONLN);
    int bindings[system_size];

    while (keep_running) {
        /* Parallel region of aprox query_interval_ms long */
        #pragma omp parallel
        {
            if (busy_wait) {
                struct timespec start_loop;
                clock_gettime(CLOCK_MONOTONIC, &start_loop);
                int interval = 0;
                do {
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    interval = timespec_diff(&start_loop, &now);
                } while(interval < query_interval_ms);
            } else {
                usleep(query_interval_ms*1000L);
            }
        }

        /* Poll DLB */
        int new_threads;
        int err = DLB_PollDROM(&new_threads, NULL);
        if (err == DLB_SUCCESS) {
            printf("\nChanging to %d threads\n", new_threads);
            omp_set_num_threads(new_threads);
        }

        /* Obtain CPU binding of all threads */
        int num_threads;
        #pragma omp parallel
        {
            #pragma omp single
            {
                num_threads = omp_get_num_threads();
            }
            bindings[omp_get_thread_num()] = sched_getcpu();
        }

        /* Print bindings info */
        printf("\rBindings: ");
        int i;
        for (i=0; i<num_threads; ++i) {
            printf("%d ", bindings[i]);
        }
        fflush(stdout);
    }
    fputc('\n', stdout);

    DLB_Finalize();
    return 0;
}
