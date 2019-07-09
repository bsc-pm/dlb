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

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <omp.h>
#include <dlb.h>

static int run = 1;

static void sighandler(int signum)
{
    if (signum == SIGINT) {
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

    printf("Starting DROM example.\n"
           "Press 'Ctrl-C' to gracefully stop the execution and clean DLB shared memories.\n"
           "PID: %d\n", getpid());

    int err = DLB_Init(0, NULL, "--drom");
    if (err == DLB_SUCCESS) {

        int num_threads = omp_get_max_threads();
        printf("Starting example with number of threads: %d\n", num_threads);

        while(run == 1) {
            if (DLB_PollDROM(&num_threads, NULL) == DLB_SUCCESS) {
                if (num_threads > 0) {
                    printf("Number of threads changed to %d\n", num_threads);
                } else if (num_threads == 0) {
                    printf("WARNING: Assigned CPUs changed to 0.\n");
                }
            }

            #pragma omp parallel num_threads(num_threads)
            {
                usleep(500000);
            }
            __sync_synchronize();
        }
    } else if (err == DLB_ERR_PERM) {
        printf("DLB_Init error:\n"
                "    There are no CPU slots left for this process.\n"
                "    Remember to remove some CPUs first from any running process with dlb_taskset\n"
                "    and then run this process again with taskset -c [cpu_list] %s.\n"
                    , argv[0]);
    } else {
        printf("DLB failed with the following error: %s\n", DLB_Strerror(err));
    }

    printf("Finalizing DROM example.\n");
    DLB_Finalize();

    return 0;
}
