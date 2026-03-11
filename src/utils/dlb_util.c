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

/* Implementation for dlb and dlb_mpi binaries */

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

#ifdef MPI_LIB
#include <mpi.h>

static int mpi_rank;

#  define RANK0_PRINTF(fmt, ...) \
    do { if (mpi_rank == 0) fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#  define ERR_PRINTF(fmt, ...) \
    fprintf(stderr, "[rank %d] " fmt, mpi_rank, ##__VA_ARGS__)

#  define ERR_ABORT() \
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE)

#else

#  define RANK0_PRINTF(fmt, ...) \
    fprintf(stdout, fmt, ##__VA_ARGS__)

#  define ERR_PRINTF(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

#  define ERR_ABORT() \
    exit(EXIT_FAILURE)

#endif


static void dlb_check(int error, const char *func) {

    if (error) {
        ERR_PRINTF("Operation %s did not succeed\n", func);
        ERR_PRINTF("Return code %d (%s)\n", error, DLB_Strerror(error));
        ERR_ABORT();
    }
}

static void version(void) {

    RANK0_PRINTF("%s\n", DLB_VERSION_STRING);
    RANK0_PRINTF("Configured with: %s\n", DLB_CONFIGURE_ARGS);
}

static void usage(const char *program, FILE *out) {

#ifdef MPI_LIB
    if (mpi_rank == 0)
#endif
    {
        fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
        fprintf(out, (
                    "usage:\n"
                    "\t%1$s --affinity\n"
                    "\t%1$s --gpu-affinity [--uuid]\n"
                    "\t%1$s --test-init\n"
                    "\t%1$s --help\n"
                    "\t%1$s --version\n"
                    "\n"
                    ), program);

        fputs("DLB binary.\n\n", out);

        fputs((
                    "Options:\n"
                    "  -a, --affinity           print process affinity\n"
                    "  -g, --gpu-affinity       print process and GPU affinity\n"
                    "  -u, --uuid               show full UUID with GPU affinity\n"
                    "  -i, --test-init          test DLB initialization\n"
                    "  -h, --help               print DLB variables and current value\n"
                    "                           (twice `-hh` for extended)\n"
                    "  -v, --version            print version info\n"
                    ), out);
    }
}

static void print_variables(int print_extended) {

#ifdef MPI_LIB
    if (mpi_rank == 0)
#endif
    {
        DLB_PrintVariables(print_extended);
    }

    if (!print_extended) {
        RANK0_PRINTF("Run dlb -hh for extended help\n");
    }
}

static void print_affinity(bool do_gpu_affinity, bool full_uuid) {

    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);

    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    char buffer[4096];
    if (do_gpu_affinity) {
        int error = DLB_GetGPUAffinity(buffer, sizeof(buffer), full_uuid);
        if (error != DLB_SUCCESS) {
            do_gpu_affinity = false;
        }
    }

    if (do_gpu_affinity) {
#ifndef MPI_LIB
        printf("DLB[%s:%d]: Process affinity: %s, gpu: %s\n",
                hostname, getpid(), mu_to_str(&process_mask), buffer);
#else
        printf("DLB[%s:%d]: Rank: %d, process affinity: %s, gpu: %s\n",
                hostname, getpid(), mpi_rank, mu_to_str(&process_mask), buffer);
#endif
    } else {
#ifndef MPI_LIB
        printf("DLB[%s:%d]: Process affinity: %s\n",
                hostname, getpid(), mu_to_str(&process_mask));
#else
        printf("DLB[%s:%d]: Rank: %d, process affinity: %s\n",
                hostname, getpid(), mpi_rank, mu_to_str(&process_mask));
#endif
    }
}


int dlb_util(int argc, char *argv[]) {

    bool do_affinity = false;
    bool do_gpu_affinity = false;
    bool do_print = false;
    bool do_version = false;
    bool print_extended = false;
    bool full_uuid = false;
    bool test_init = false;

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"affinity",     no_argument,    NULL, 'a'},
        {"gpu-affinity", no_argument,    NULL, 'g'},
        {"uuid",         no_argument,    NULL, 'u'},
        {"test-init",    no_argument,    NULL, 'i'},
        {"help",         no_argument,    NULL, 'h'},
        {"help-extra",   no_argument,    NULL, 'x'}, /* kept for compatibility */
        {"version",      no_argument,    NULL, 'v'},
        {0,              0,              NULL,  0 }
    };

    while ((opt = getopt_long(argc, argv, "aguihxv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a':
                do_affinity = true;
                break;
            case 'g':
                do_affinity = true;
                do_gpu_affinity = true;
                break;
            case 'u':
                full_uuid = true;
                break;
            case 'i':
                test_init = true;
                break;
            case 'h':
                if (!do_print) {
                    do_print = true;
                } else {
                    print_extended = true;
                }
                break;
            case 'x':
                do_print = true;
                print_extended = true;
                break;
            case 'v':
                do_version = true;
                break;
            default:
                usage(argv[0], stderr);
                ERR_ABORT();
        }
    }

#ifdef MPI_LIB
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
#endif

    /* --test-init can be used in combination with any flag */
    if (test_init) {
        int error = DLB_Init(0, NULL, NULL);
        dlb_check(error, __FUNCTION__);
    }

    /* These flags are mutually exclusive, checked in order of preference: */
    if (do_version) {
        version();
    } else if (do_print) {
        print_variables(print_extended);
    } else if (do_affinity) {
        print_affinity(do_gpu_affinity, full_uuid);
    } else if (!test_init) {
        usage(argv[0], stdout);
    }

    if (test_init) {
        int error = DLB_Finalize();
        dlb_check(error, __FUNCTION__);
    }

#ifdef MPI_LIB
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif

    return EXIT_SUCCESS;
}
