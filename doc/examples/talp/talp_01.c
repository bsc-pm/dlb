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
#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <dlb.h>
#include <dlb_talp.h>
#include <sys/time.h>
#include <stdlib.h>
#include <mpi.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

static int run = 1;
#define TIME 500000

static void sighandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        run = 0;
    }
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = &sighandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    MPI_Init(&argc,&argv);

    int me, how_many;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &how_many);

    pid_t pid = getpid();
    printf("Starting TALP example.\n"
            "Press 'Ctrl-C' to gracefully stop the execution and clean DLB shared memories.\n"
        "PID: %d\n", pid);

    double mpi_time, cpu_time;
    int num_iterations = 0;

    while(run) {
        if (me == 0) usleep(TIME);
        else usleep(TIME/4);

        MPI_Barrier(MPI_COMM_WORLD);

        DLB_TALP_MPITimeGet(pid, &mpi_time);
        DLB_TALP_CPUTimeGet(pid, &cpu_time);

        ++num_iterations;
        if (num_iterations%10 == 0) {
            printf("%d:MPI TIME: %f ", pid, mpi_time);
            printf("%d:CPU TIME: %f\n", pid, cpu_time);
        }
    }

    printf("Finalizing TALP example.\n");

    /* Explicitly finalize DLB before MPI because some implementations
     * may not return from MPI if the process is signaled */
    DLB_Finalize();

    MPI_Finalize();

    return 0;
}
