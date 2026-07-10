#include <dlb_talp.h>
#include <mpi.h>
#include <omp.h>
#include <stdint.h>
#include <stdlib.h>

static void busy_wait(double seconds) {
    double start = omp_get_wtime();
    volatile uint64_t x = 0;
    while (omp_get_wtime() - start < seconds) x++;
}

void run_test(int rank) {
    /*
     * Pattern (2 MPI ranks, 2 OMP threads each):
     *
     * Both ranks run a parallel region with load imbalance,
     * then synchronize via MPI with asymmetric timing to produce
     * measurable values in all four time categories.
     *
     *          thread0  thread1
     * Rank 0:  [=1.0s=] [=0.5s=]  → imbalance: 0.5s
     * Rank 1:  [=0.5s=] [=0.25s=] → imbalance: 0.25s
     *
     * After the parallel region, rank 0 arrives at MPI ~0.5s after rank 1.
     *
     * Serial section before parallel: 0.5s → serialization for worker threads.
     */

    /* Serial section */
    busy_wait(0.5);

    #pragma omp parallel num_threads(2)
    {
        /* Parallel region with imbalance */
        double work = (rank == 0)
            ? (omp_get_thread_num() == 0 ? 1.0 : 0.5)
            : (omp_get_thread_num() == 0 ? 0.5 : 0.25);

        busy_wait(work);
    }

    /* MPI phase: rank 0 took 1.0s in parallel → arrives 0.5s later than rank 1 */
    int dummy = 0;
    MPI_Status status;
    if (rank == 0) {
        MPI_Send(&dummy, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&dummy, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
    } else {
        MPI_Recv(&dummy, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); /* waits ~0.5s */
        MPI_Send(&dummy, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
}

int main(int argc, char *argv[]) {

    int repetitions = argc > 1 ? atoi(argv[1]) : 1;

    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* We measure a new region to discard MPI initialization after OMPT init */
    dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Test");
    DLB_MonitoringRegionStart(monitor);

    for (int i = 0; i < repetitions; ++i) {
        run_test(rank);
    }

    DLB_MonitoringRegionStop(monitor);

    MPI_Finalize();
    return 0;
}
