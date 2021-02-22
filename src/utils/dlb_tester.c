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

/* This program will indefinitely run until some termination signal is captured */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <time.h>
#include <omp.h>

#include "apis/dlb.h"
#include "support/mask_utils.h"

enum { QUERY_INTERVAL_DEFAULT_MS = 1000 };

static volatile int keep_running = 1;

static void sighandler(int signum) {
    printf("Received signal %d, dying\n", signum);
    keep_running = 0;
}

static int64_t get_time_in_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_nsec / 1000000L + (int64_t)ts.tv_sec * 1000L;
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB tester\n");
    fputs("Simulate a DLB program, print binding info at the specified interval.\n\n", out);

    fprintf(out, "usage: %s [OPTIONS]\n", program);
    fputs((
                "Options:\n"
                "  -b, --busy-wait              force 100%% CPU usage while waiting\n"
                "  -i, --interval <ms>          amount of milliseconds between each update\n"
                "  -l, --lend-cpus[=<cpuset>]   lend CPUs to LeWI, defaults to all\n"
                "      --lend-interval <ms>     if specified, interval to reclaim lent CPUs\n"
                "  -h, --help                   print this help\n"
                "\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    /* Obtain process mask */
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(process_mask), &process_mask);

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

    /* Long options that have no corresponding short option */
    enum {
        LEND_INTERVAL_OPTION = CHAR_MAX + 1
    };

    /* Parse options */
    bool busy_wait = false;
    int query_interval_ms = QUERY_INTERVAL_DEFAULT_MS;
    bool lewi = false;
    cpu_set_t cpus_to_lend;
    int lend_cpus_interval = 0;
    int opt;
    extern char *optarg;
    struct option long_options[] = {
        {"busy-wait",     no_argument,       NULL, 'b'},
        {"interval",      required_argument, NULL, 'i'},
        {"lend-cpus",     optional_argument, NULL, 'l'},
        {"lend-interval", required_argument, NULL, LEND_INTERVAL_OPTION},
        {"help",          no_argument,       NULL, 'h'},
        {0,               0,                 NULL, 0 }
    };
    while ((opt = getopt_long(argc, argv, "bi:l::h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'b':
                busy_wait = true;
                break;
            case 'i':
                query_interval_ms = strtol(optarg, NULL, 0);
                if (!query_interval_ms)
                    query_interval_ms = QUERY_INTERVAL_DEFAULT_MS;
                break;
            case 'l':
                lewi = true;
                if (optarg) {
                    printf("optarg: %s\n", optarg);
                    mu_parse_mask(optarg, &cpus_to_lend);
                } else {
                    memcpy(&cpus_to_lend, &process_mask, sizeof(process_mask));
                }
                break;
            case LEND_INTERVAL_OPTION:
                lend_cpus_interval = strtol(optarg, NULL, 0);
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
    if (lend_cpus_interval > 0) {
        printf("LeWI interval:  %d ms\n", lend_cpus_interval);
    }

    /* Initialize DLB */
    DLB_Init(0, &process_mask, "--drom --lewi");
    int system_size = sysconf(_SC_NPROCESSORS_ONLN);
    int *bindings = malloc(sizeof(int)*system_size);

    /* Lend CPUs to LeWI on first iteration only if interval is not specified */
    bool lewi_lend = lend_cpus_interval == 0;
    bool lewi_reclaim = false;
    bool lewi_has_lent = false;
    int64_t lewi_timestamp = get_time_in_ms();

    while (keep_running) {
        /* Parallel region of aprox query_interval_ms long */
        #pragma omp parallel
        {
            if (busy_wait) {
                int64_t start_loop = get_time_in_ms();
                int64_t interval = 0;
                do {
                    interval = get_time_in_ms() - start_loop;
                } while(interval < query_interval_ms);
            } else {
                usleep(query_interval_ms*1000L);
            }
        }

        /* Poll DLB */
        int new_threads;
        int err = DLB_PollDROM(&new_threads, &process_mask);
        if (err == DLB_SUCCESS) {
            printf("\nChanging to %d threads\n", new_threads);
            omp_set_num_threads(new_threads);
            lewi_lend = true;
        }

        /* LeWI */
        if (lewi) {
            // Compute lend cpus interval
            if (lend_cpus_interval > 0) {
                int64_t now = get_time_in_ms();
                if (now - lewi_timestamp > lend_cpus_interval) {
                    lewi_timestamp = now;
                    lewi_lend = !lewi_has_lent;
                    lewi_reclaim = lewi_has_lent;
                    lewi_has_lent = !lewi_has_lent;
                }
            }

            // Lend
            if (lewi_lend) {
                printf("\nLending mask %s\n", mu_to_str(&cpus_to_lend));
                // create mask and call DLB
                cpu_set_t mask;
                CPU_AND(&mask, &process_mask, &cpus_to_lend);
                DLB_LendCpuMask(&mask);
                // update num threads
                cpu_set_t active_mask;
                CPU_XOR(&active_mask, &process_mask, &mask);
                CPU_AND(&active_mask, &process_mask, &active_mask);
                omp_set_num_threads(CPU_COUNT(&active_mask));
            } else if (lewi_reclaim) {
                printf("\nReclaiming\n");
                DLB_Reclaim();
                omp_set_num_threads(CPU_COUNT(&process_mask));
            }

            // reset flags
            lewi_lend = false;
            lewi_reclaim = false;
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

    free(bindings);
    DLB_Finalize();
    return 0;
}
