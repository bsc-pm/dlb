
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <float.h>
#include <mpi.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef DLB
#include <dlb.h>
#endif

int main(int argc, char *argv[]) {

    MPI_Init(&argc, &argv);

#ifdef DLB
    DLB_Init(0, NULL, NULL);

    /* Initialize two named barriers, with LeWI off because it is not enable in this example */
    dlb_barrier_t* barrier1 = DLB_BarrierNamedRegister("Barrier_1", DLB_BARRIER_LEWI_OFF);
    dlb_barrier_t* barrier2 = DLB_BarrierNamedRegister("Barrier_2", DLB_BARRIER_LEWI_OFF);

    /* DLB requires every process to finalize DLB_Init before anyone calls DLB_Barrier. */
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int nrank;
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);

    /* test_second splits communicator with color 1 */
    int color = 1;

    MPI_Comm MPI_COMM_WORLD_SECOND = MPI_COMM_NULL;
    MPI_Comm_split(MPI_COMM_WORLD, color, rank, &MPI_COMM_WORLD_SECOND);

    int rank_second;
    MPI_Comm_rank(MPI_COMM_WORLD_SECOND, &rank_second);

    int nrank_second;
    MPI_Comm_size(MPI_COMM_WORLD_SECOND, &nrank_second);

    printf("test_second: Rank #rank_second = %d of nrank_second = %d,"
            " with rank = %d and nrank = %d\n",
            rank_second, nrank_second, rank, nrank);

    /* Synchronize init messages */
    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();


    // Application that loops while waiting.

    /* Compute - Start */

    long int number_of_elements = 1000000000;
    double coefficient = 3.2 / number_of_elements;
    double sum = 0.;

    while(1) {

#ifdef DLB
        /* wait on barrier1 so that test_first computes its iteration */
        DLB_BarrierNamed(barrier1);
#else
        /* wait until test_first computes its iteration */
        MPI_Barrier(MPI_COMM_WORLD);
#endif

        /* 1: Read dnum from test_first.txt */
        double dnum = 0.;
        if (rank_second == 0) {

            FILE *file_ptr;
            file_ptr = fopen("./test_first.txt","r");
            fscanf(file_ptr,"%lf", &dnum);
            fclose(file_ptr);

            if (dnum < DBL_EPSILON) {
                printf("test_second: Detected END_MARK. Quitting.\n", dnum);
            } else {
                printf("test_second: Value extracted from first simulation = %lf\n", dnum);
            }
        }

        MPI_Bcast(&dnum, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD_SECOND);

        if (dnum < DBL_EPSILON) {
            break;
        }

        /* 2: Compute */

        if (rank_second == 0) {
            printf("************************************************\n");
            printf("test_second: computing\n");
            printf("************************************************\n");
        }

        double sum_tmp = 0;

        for (long int i = 0; i < number_of_elements; i++) {
            sum_tmp += i * coefficient * coefficient;
        }

        printf("test_second: Partial sum: %lf at rank_second = %d\n", sum_tmp, rank_second);

        MPI_Reduce(&sum_tmp, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD_SECOND);

        /* 3: Write sum to test_second.txt  */

        if (rank_second == 0) {
            sum += dnum;
            printf("test_second: Total sum: %lf \n", sum);

            FILE *file_ptr;
            file_ptr = fopen("test_second.txt", "w");
            fprintf(file_ptr, "%lf", sum);
            fclose(file_ptr);
            printf("test_second: Writing sum to file test_second.txt \n");
        }

#ifdef DLB
        /* barrier2 unblocks test_first so it can continue its execution */
        DLB_BarrierNamed(barrier2);
#else
        /* unblock test_first so it can continue its execution */
        MPI_Barrier(MPI_COMM_WORLD);
#endif
    }

    /* Final sinchronization to notify test_first that we are out of the loop */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Compute - End */

    double end = MPI_Wtime();

    if (rank_second == 0) {
        remove("test_second.txt");
        printf("test_second is finished. It ran in %lf seconds\n", end - start);
    }

#ifdef DLB
    DLB_Finalize();
#endif

    MPI_Finalize();
}
