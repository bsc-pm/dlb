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
 *      <B>dlb_mpi</B> {--affinity | --version}
 *  \section description DESCRIPTION
 *      Utility command to display process affinity with MPI rank.
 *
 *      <DL>
 *          <DT>-a, --affinity</DT>
 *          <DD>Print process affinity.</DD>
 *
 *          <DT>-v, --version</DT>
 *          <DD>Print version info.</DD>
 *      </DL>
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (dlb@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb "dlb"(1)
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
#include <mpi.h>


static void __attribute__((__noreturn__)) version(void) {
    fprintf(stdout, "%s\n", DLB_VERSION_STRING);
    fprintf(stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS);
    exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
    fprintf(out, (
                "usage:\n"
                "\t%1$s --affinity\n"
                "\t%1$s --version\n"
                "\n"
                ), program);

    fputs("DLB binary.\n\n", out);

    fputs((
                "Options:\n"
                "  -a, --affinity           print process affinity\n"
                "  -v, --version            print version info\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) print_affinity(void) {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);

    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    printf("DLB[%s:%d]: Rank: %d, process affinity: %s\n",
            hostname, getpid(), mpi_rank, mu_to_str(&process_mask));

    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {

    MPI_Init(&argc, &argv);

    bool do_affinity = false;

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"affinity",    no_argument,    NULL, 'a'},
        {"version",     no_argument,    NULL, 'v'},
        {0,             0,              NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, "av", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a':
                do_affinity = true;
                break;
            case 'v':
                version();
                break;
            default:
                usage(argv[0], stderr);
        }
    }

    if (do_affinity) {
        print_affinity();
    } else {
        usage(argv[0], stderr);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
