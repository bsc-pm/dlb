
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

    if (rank == 0) {
        printf("test_first: Total number of ranks, nrank = %d\n", nrank);
    }

    /* test_first splits communicator with color 0 */
    int color = 0;

    MPI_Comm MPI_COMM_WORLD_FIRST = MPI_COMM_NULL;
    MPI_Comm_split(MPI_COMM_WORLD, color, rank, &MPI_COMM_WORLD_FIRST);

    int rank_first;
    MPI_Comm_rank(MPI_COMM_WORLD_FIRST, &rank_first);

    int nrank_first;
    MPI_Comm_size(MPI_COMM_WORLD_FIRST, &nrank_first);

    printf("test_first: Rank #rank_first = %d of nrank_first = %d,"
            " with rank = %d and nrank = %d\n",
            rank_first, nrank_first, rank, nrank);

    /* Synchronize init messages */
    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();

    /* Compute - Start loop */

    long int number_of_elements = 1000000000;
    double coefficient = 2.5 / number_of_elements;
    int max_iterations = 3;

    double sum = 0.;

    for (int j = 0; j < max_iterations; j++) {

        if (rank_first == 0) {
            printf("================================================\n");
            printf("test_first: iteration = %d\n", j);
            printf("================================================\n");
        }

        /* 1: Read dnum from test_second.txt, if it exists */

        double dnum = 0.;
        if (rank_first == 0) {

            if (access("test_second.txt", F_OK) == 0) {
                FILE *file_ptr;
                file_ptr = fopen("test_second.txt","r");
                fscanf(file_ptr,"%lf", &dnum);
                fclose(file_ptr);

                printf("test_first: Value extracted from second simulation = %lf\n", dnum);
            }
            else {
                printf("test_first: The file test_second.txt does not exist and dnum = %lf\n",
                        dnum);
            }
        }

        MPI_Bcast(&dnum, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD_FIRST);

        /* 2: Compute */

        double sum_tmp = 0.;

        for (long int i = 0; i < number_of_elements; i++) {
            sum_tmp += i * coefficient * coefficient;
        }

        printf("test_first: Partial sum: %lf at rank_first = %d\n", sum_tmp, rank_first);

        MPI_Reduce(&sum_tmp, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD_FIRST);

        /* 3: Write sum to test_first.txt  */

        if (rank_first == 0) {
            sum += dnum;
            printf("test_first: Total sum: %lf for iterations #i = %d\n", sum, j);
            printf("test_first: Writing sum to file test_first.txt \n");

            FILE *file_ptr;
            file_ptr = fopen("test_first.txt", "w");
            fprintf(file_ptr, "%lf", sum);
            fclose(file_ptr);
        }

#ifdef DLB
        /* barrier1 unblocks test_second so that it can start its iteration */
        DLB_BarrierNamed(barrier1);

        /* barrier2 is used for waiting until test_second finishes its iteration */
        DLB_BarrierNamed(barrier2);
#else
        /* unblock test_second so that it can start its iteration */
        MPI_Barrier(MPI_COMM_WORLD);

        /* block until test_second finishes its iteration */
        MPI_Barrier(MPI_COMM_WORLD);
#endif

    }

    /* Obtain final value from test_second.txt and finalize test_second program */
    if (rank_first == 0) {
        double dnum;
        FILE *file_ptr;
        file_ptr = fopen("test_second.txt","r");
        fscanf(file_ptr,"%lf", &dnum);
        fclose(file_ptr);

        printf("test_first: Final value extracted from second simulation = %lf\n", dnum);
        printf("test_first: Writing END_MARK to file test_first.txt \n");

        file_ptr = fopen("test_first.txt", "w");
        fprintf(file_ptr, "0.0");
        fclose(file_ptr);
    }

    /* Synchronize with test_second one last time */
#ifdef DLB
    DLB_BarrierNamed(barrier1);
#else
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    /* Final sinchronization to make sure test_second has read the file and break the loop */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Compute - End */

    double end = MPI_Wtime();

    if (rank_first == 0) {
        remove("test_first.txt");
        printf("test_first is finished. It ran in %lf seconds\n", end - start);
    }

#ifdef DLB
    DLB_Finalize();
#endif

    MPI_Comm_free(&MPI_COMM_WORLD_FIRST);

    MPI_Finalize();
}
