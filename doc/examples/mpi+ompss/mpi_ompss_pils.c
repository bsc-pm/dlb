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

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <mpi.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static const char DEFAULT_LOADS[] = "1000,2500";
static double DEFAULT_PARALL_GRAIN = 1.0;
static int DEFAULT_LOOPS = 10;
static int DEFAULT_TASK_DURATION_IN_US = 100;

static int64_t usecs (void) {
    struct timeval t;
    gettimeofday(&t,NULL);
    return t.tv_sec*1000000+t.tv_usec;
}

static int computation (int num, int usec) {
    float a=0.99999f;
    float p=num;
    int i, x;
    x=145;

    int64_t t_start = usecs();
    int64_t t_now;
    do {
        for(i=0; i<x; i++) {
            p+=a*i;
            p=p/i;
        }
        t_now = usecs();
    } while (t_now - t_start < usec);

    return (int)p;
}

static double LoadUnbalance(int loads[], int mpi_size) {
    int sum=0;
    int max=0;
    int i;
    for (i=0; i<mpi_size; i++){
        sum+=loads[i];
        max=MAX(max, loads[i]);
    }
    return (double)sum / (mpi_size*max);
}

#pragma omp task
static void iter(double *iter_time, int *fib, int usec) {
    int64_t t_start, t_end;

    t_start=usecs();
    *fib=computation(35, usec);
    t_end=usecs();

    double local_iter_time = (t_end-t_start)/1000000.0;
    #pragma omp atomic
    *iter_time += local_iter_time;
}

static int parse_load_list(int loads[], int mpi_size, const char *arg) {
    int error = 0;
    int rank_id = 0;

    /* Tokenize a copy of arg */
    size_t len = strlen(arg) + 1;
    char *arg_copy = malloc(sizeof(char)*len);
    strcpy(arg_copy, arg);
    char *saveptr = NULL;
    char *token = strtok_r(arg_copy, ",", &saveptr);
    while (token && rank_id < mpi_size) {
        /* Tokenize a possible load*repetion */
        char *saveptr2 = NULL;
        char *load_str = strtok_r(token, "*", &saveptr2);
        char *n_reps_str = strtok_r(NULL, "*", &saveptr2);

        int load = strtol(load_str, NULL, 10);
        if (load < 0) {
            fprintf(stderr, "Negative load is not allowed\n");
            error = -1;
            break;
        }

        int n_reps = 1;
        if (n_reps_str != NULL) {
            n_reps = strtol(n_reps_str, NULL, 10);
        }

        int i;
        for (i=0; i<n_reps; ++i) {
            loads[rank_id++] = load;
        }

        token =  strtok_r(NULL, ",", &saveptr);
    }

    /* Offset of positions provided vs filled */
    int offset = rank_id;

    /* Fill rest of the positions */
    for (; rank_id < mpi_size; ++rank_id) {
        loads[rank_id] = loads[rank_id-offset];
    }

    free(arg_copy);

    return error;
}

static int parse_load_file(int loads[], int mpi_size, const char *arg) {
    int error = 0;
    int rank_id = 0;

    FILE* load_file = fopen(arg,"r");
    if (load_file == NULL) {
        fprintf(stderr, "Error opening file %s\n", arg);
        return -1;
    }

    /* Read load from file */
    int i;
    for (i=0; i<mpi_size; ++i) {
        if (fscanf(load_file, "%d\n", &loads[rank_id]) == EOF ) break;
        ++rank_id;
    }

    /* Offset of positions provided vs filled */
    int offset = rank_id;

    /* Fill rest of the positions */
    for (; rank_id < mpi_size; ++rank_id) {
        loads[rank_id] = loads[rank_id-offset];
    }
    return error;
}

static void __attribute__((__noreturn__)) usage(const char *program, int mpi_rank, FILE *out) {
    if (mpi_rank == 0) {
        fprintf(out, (
                    "PILS usage:\n"
                    "\t%s [options]\n"
                    "\n"
                    ), program);

        fputs((
                    "Options:\n"
                    "  -l, --loads <n1,n2*m,...>  comma separated values of load per rank\n"
                    "  -f, --loads-file <file>    file path with load per rank, separated in lines\n"
                    "  -g, --grain <double>       parallelism grain parameter: (0.0,1.0]\n"
                    "  -i, --iterations <n>       number of iterations\n"
                    "  -t, --task-duration <n>    task duration in microseconds\n"
                    "  -v, --verbose              print iteration times per process\n"
                    "  -h, --help                 print this help\n"
                    "\n"
                    ), out);

        fprintf(out, (
                    "Default values:\n"
                    "  Loads:         %s\n"
                    "  Grain:         %.1f\n"
                    "  Iterations:    %d\n"
                    "  Task duration: %d us\n"
                    ),
                DEFAULT_LOADS,
                DEFAULT_PARALL_GRAIN,
                DEFAULT_LOOPS,
                DEFAULT_TASK_DURATION_IN_US);
    }

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}


int main(int argc, char* argv[]) {

    int mpi_rank, mpi_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    int *loads = malloc(sizeof(int)*mpi_size);

    parse_load_list(loads, mpi_size, DEFAULT_LOADS);
    double parall_grain = DEFAULT_PARALL_GRAIN;
    int loops = DEFAULT_LOOPS;
    int task_usec = DEFAULT_TASK_DURATION_IN_US;
    bool verbose = false;

    int opt;
    extern char *optarg;
    struct option long_options[] = {
        {"loads",           required_argument,  NULL, 'l'},
        {"loads-file",      required_argument,  NULL, 'f'},
        {"grain",           required_argument,  NULL, 'g'},
        {"iterations",      required_argument,  NULL, 'i'},
        {"task-duration",   required_argument,  NULL, 't'},
        {"verbose",         no_argument,        NULL, 'v'},
        {"help",            no_argument,        NULL, 'h'},
        {0,                 0,                  NULL, 0}
    };

    while ((opt = getopt_long(argc, argv, "l:f:g:i:t:vh", long_options, NULL)) != -1) {
        switch(opt) {
            case 'l':
                if (parse_load_list(loads, mpi_size, optarg) == -1) {
                    usage(argv[0], mpi_rank, stderr);
                }
                break;
            case 'f':
                if (parse_load_file(loads, mpi_size, optarg) == -1) {
                    usage(argv[0], mpi_rank, stderr);
                }
                break;
            case 'g':
                parall_grain = strtod(optarg, NULL);
                break;
            case 'i':
                loops = strtol(optarg, NULL, 10);
                break;
            case 't':
                task_usec = strtol(optarg, NULL, 10);
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                usage(argv[0], mpi_rank, stdout);
                break;
            default:
                usage(argv[0], mpi_rank, stderr);
                break;
        }
    }

    if (parall_grain>1 || parall_grain<=0) {
        fprintf(stderr,
                "Error: Parallelism grain parameter must be a value between 1 and 0 (but not 0)\n"
                "Provided: %.1f\n",
                parall_grain);
        exit(EXIT_FAILURE);
    }

    if (mpi_rank == 0) {
        double LB = LoadUnbalance(loads, mpi_size);
        fprintf(stdout, "LB: %.2f \n", LB);
    }

    int steps = loads[mpi_rank];
    fprintf(stdout, "%d: Load = %d\n", mpi_rank, steps);

    int BS = steps * parall_grain;
    if (BS <= 0) BS = 1;
    fprintf(stdout, "%d: BS = %d\n", mpi_rank, BS);

    int fib;
    double iter_time = 0;
    double total_time, app_time;

    int64_t t_start, t_end;
    t_start = usecs();


    /***********************************************************************/
    /************** MAIN LOOP **********************************************/
    int k;
    for(k=0; k<loops; k++) {

        int i;
        for (i=0; i<steps; i+=BS) {

            int upper_bound = MIN(steps, i+BS);

            int j;
            for(j=i; j<upper_bound; j++){
                iter(&iter_time, &fib, task_usec);
            }
        }
        #pragma omp taskwait

        // Total computation time for each process
        if (verbose) {
            printf("%d: Local time: %6.3f\n", mpi_rank, iter_time);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // Add computation time of whole application
        MPI_Allreduce(&iter_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    }

    if (mpi_rank == 0) printf("Total CPU time: %6.3f s\n", total_time);

    // Application elapsed time
    t_end = usecs();
    app_time = (t_end-t_start)/1000000.0;
    if (mpi_rank == 0) {
      printf("\nApplication time = %.1f s\n", app_time);
    }

    MPI_Finalize();

    free(loads);

    return EXIT_SUCCESS;
}
