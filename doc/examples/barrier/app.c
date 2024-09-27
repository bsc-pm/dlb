
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <mpi.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef DLB
#include <dlb.h>
#endif

static void __attribute__((noreturn)) usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s 0|1 \n", prog_name);
    exit(EXIT_FAILURE);
}

static int64_t usecs (void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000000+t.tv_usec;
}

static int computation (int usec) {
    float a = 0.99999f;
    float p = 35.f;
    int x = 145;

    int64_t t_start = usecs();
    int64_t t_now;
    do {
        for(int i=0; i<x; i++) {
            p+=a*i;
            p=p/i;
        }
        t_now = usecs();
    } while (t_now - t_start < usec);

    return (int)p;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        usage(argv[0]);
    }

    /* Get coupling id */
    int coupling_id = strtol(argv[1], NULL, 10);
    if (coupling_id != 0 && coupling_id != 1) {
        usage(argv[0]);
    }

    MPI_Init(&argc, &argv);

#ifdef DLB
    DLB_Init(0, NULL, NULL);

    /* DLB requires every process to finalize DLB_Init before anyone calls DLB_Barrier. */
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    /* Get rank id */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Get process affinity mask */
    cpu_set_t cpu_set;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);

    /* Print rank id, coupling id, and process affinity mask */
    int nproc_onln = sysconf(_SC_NPROCESSORS_ONLN);
    printf("RANK %d, coupling %d: [ ", rank, coupling_id);
    for (int i = 0; i < nproc_onln; ++i) {
        if (CPU_ISSET(i, &cpu_set)) {
             printf("%d ", i);
        }
    }
    printf("]\n");

    enum { NUM_ITERATIONS = 10 };
    enum { US_PER_ITERATION = 1000000 };

    double start_time;
    if (rank == 0) {
        start_time = MPI_Wtime();
    }

    int p;

    /* Application main iteration */
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        /* Emulate a coupling by alternating 'useful' iterations */
        if (i % 2 == coupling_id) {
            p += computation(US_PER_ITERATION);
        }
#ifdef DLB
        DLB_Barrier();
#endif
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) {
        printf("Elapsed time: %f s\n", MPI_Wtime() - start_time);
    }

#ifdef DLB
    DLB_Finalize();
#endif

    MPI_Finalize();
}
