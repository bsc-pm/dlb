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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <dlb.h>
#include <dlb_talp.h>
#include <mpi.h>

#define TIME 500000

int main(int argc, char *argv[])
{
    MPI_Init(&argc,&argv);

    int mpi_rank, mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    pid_t pid = getpid();

    int num_iterations = 20;
    if (argc > 1) {
        int num = strtol(argv[1], NULL, 10);
        if (errno != 0 && num > 0) {
            num_iterations = num;
        }
    }

    if (mpi_rank == 0) {
        printf("Starting TALP example.\n"
                "This program simulates an imbalanced MPI program:\n"
                " - MPI and Useful times are printed every 10 iterations.\n"
                " - Each iteration takes about 0.5s.\n"
                " - The program accepts an optional parameter to set the number\n"
                "   of iterations, default: 20\n"
                "Number of iterations: %d\n",
                num_iterations);
    }

    int i;
    for (i=0; i< num_iterations; ++i) {
        if (mpi_rank == 0) {
            // Rank 0 causes imbalance
            usleep(TIME);
        }
        else {
            usleep(TIME/4);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if ((i+1)%10 == 0) {
            double mpi_time, useful_time;
            DLB_TALP_GetTimes(pid, &mpi_time, &useful_time);
            printf("Rank: %d, PID: %d, MPI time: %f, Useful time: %f\n",
                    pid, mpi_rank, mpi_time, useful_time);
        }
    }

    if (mpi_rank == 0) {
        printf("Finalizing TALP example.\n");
    }

    MPI_Finalize();

    return 0;
}
