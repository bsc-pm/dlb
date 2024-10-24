/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

/* This programs simulates an MPI application, where TOTAL_DURATION,
 * MPI_FREQUENCY, and MPI_DURATION may be configured: */

enum { TOTAL_DURATION =  5 };   /* Program duration (seconds) */
enum { MPI_FREQUENCY = 1 };     /* Time between MPI collective calls (seconds) */
enum { MPI_DURATION = 100000};  /* MPI call duration for rank 0 (microseconds) */

#include <mpi.h>
#include <time.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    struct timespec now = begin;

    while (begin.tv_sec + TOTAL_DURATION > now.tv_sec) {
        /* Time between MPI collective calls */
        if (mpi_rank == 0) {
            usleep(MPI_FREQUENCY*1e6 - MPI_DURATION);
        } else {
            sleep(MPI_FREQUENCY);
        }

        /* MPI blocking call */
        MPI_Barrier(MPI_COMM_WORLD);

        /* Update total duration */
        clock_gettime(CLOCK_MONOTONIC, &now);
    }

    MPI_Finalize();

    return 0;
}
