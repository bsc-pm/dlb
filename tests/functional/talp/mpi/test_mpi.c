#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void busy_wait(double seconds) {
    double start = MPI_Wtime();
    volatile uint64_t x = 0;
    while (MPI_Wtime() - start < seconds)
        x++;
}

void run_test(int rank) {
    int dummy = 42;
    MPI_Status status;

    if (rank == 0) {
        busy_wait(1.0);          /* 1.0s useful computation  */
        MPI_Send(&dummy, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);   /* leaves immediately */
        MPI_Recv(&dummy, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status); /* waits ~1.0s */
        /* rank 0 MPI time ≈ 1.0s (waiting for rank 1 to send back) */
    } else {
        /* rank 1: 0.5s useful, then enters Recv immediately */
        /* it will wait (1.0 - 0.5) = 0.5s for rank 0's Send, then does its work */
        busy_wait(0.5);          /* 0.5s useful computation  */
        MPI_Recv(&dummy, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); /* waits ~0.5s */
        busy_wait(1.0);          /* another 1.0s so rank 0 waits 1.0s for the reply */
        MPI_Send(&dummy, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
}

int main(int argc, char *argv[]) {

    int repetitions = argc > 1 ? atoi(argv[1]) : 1;

    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (int i = 0; i < repetitions; ++i) {
        run_test(rank);
    }

    MPI_Finalize();
    return 0;
}
