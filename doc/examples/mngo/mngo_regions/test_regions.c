#include <mpi.h>
#include <dlb.h>
#include <dlb_mngo.h>
#include <dlb_talp.h>

#define N 1000

/* Global variables corresponding to the Fortran module variables */
void *dlb_region = NULL, *monitor = NULL;
void *dlb_region_in = NULL, *monitor_in = NULL;

int ierror, comm_size, comm_rank;
int a[N][N];

/* Prepare the arrays used in the test_mngo function with some dummy data */
void prepare_mngo_test(void) {
    int i, j;

    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

    #pragma omp parallel for default(shared) private(j)
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            /* Fortran: a(j,i) = j*5000 + i with j,i starting at 1 */
            a[j][i] = (j + 1) * 5000 + (i + 1);
        }
    }
}

/* Execute some MPI imbalanced regions annotated with MNGO */
void test_mngo(void) {
    int i, j, k;

    #pragma omp parallel for default(shared) private(j, k)
    for (i = 1; i < N - 1; i++) {
        for (j = 1; j < N - 1; j++) {
            for (k = 0; k < 20; k++) {
                a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
            }
        }
    }

    dlb_region = DLB_MNGO_RegionRegister("Region 1");

    MPI_Barrier(MPI_COMM_WORLD);
    ierror = DLB_MNGO_RegionStart(dlb_region);

    #pragma omp parallel for default(shared) private(j, k) schedule(dynamic)
    for (i = 1; i < N - 1; i++) {
        for (j = 1; j < N - 1; j++) {
            a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;

            if (comm_rank == 2) {
                for (k = 0; k < 20; k++) {
                    a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
                }
            }

            if (comm_rank >= 3) {
                for (k = 0; k < 10; k++) {
                    a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ierror = DLB_MNGO_RegionStop(dlb_region);

    dlb_region = DLB_MNGO_RegionRegister("Region 2");

    MPI_Barrier(MPI_COMM_WORLD);
    ierror = DLB_MNGO_RegionStart(dlb_region);

    #pragma omp parallel for default(shared) private(j, k) schedule(dynamic)
    for (i = 1; i < N - 1; i++) {
        for (j = 1; j < N - 1; j++) {
            a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;

            if (comm_rank < 2) {
                for (k = 0; k < 200; k++) {
                    a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
                }
            }

            if (comm_rank == 2) {
                for (k = 0; k < 10; k++) {
                    a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
                }
            }
        }

        for (j = 1; j < N - 1; j++) {
            for (k = 0; k < 20; k++) {
                a[j][i] = (a[j - 1][i] + a[j][i - 1] + a[j + 1][i] + a[j][i + 1]) / 4;
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ierror = DLB_MNGO_RegionStop(dlb_region);
}

int main(void) {
    int i;

    MPI_Init(NULL, NULL);

    prepare_mngo_test();

    for (i = 0; i < 5; i++) {
        test_mngo();
    }

    MPI_Finalize();
    return 0;
}
