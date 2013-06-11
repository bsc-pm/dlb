
/*<testinfo>
test_generator="gens/mpi_omp-generator"
</testinfo>*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <sched.h>
#include <utmpx.h>
#include <omp.h>
#include "dlb/DLB_interface.h"

#define N 8192
#define M 256


int size, rank;
double *A, *B, *C;

void compute(int n)
{
    int i;
    for (i=n; i<n+M; i++) {
        A[i] = i*i;
        B[i] = i*7;
    }

    for (i=n; i<n+M; i++) {
        C[i] = A[i] / B[i];
    }
}

void short_task(int n)
{
    int i;
    for (i=0; i<N; i++) compute(n);
}

void long_task(int n)
{
    int i;
    for (i=0; i<2*N; i++) compute(n);
}

int main(int argc, char* argv[])
{
   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   A = (double * ) malloc(sizeof(double) * N * size);
   B = (double * ) malloc(sizeof(double) * N * size);
   C = (double * ) malloc(sizeof(double) * N * size);

   MPI_Barrier(MPI_COMM_WORLD);

   int i,j;
   for (j = 0; j < 1; j++) {
      if (rank%2 == 0) {
         #pragma omp parallel for
         for(i=0;i<16;i++) short_task(i*M*(rank+1));
      }
      else {
         #pragma omp parallel for
         for(i=0;i<32;i++) long_task(i*M*(rank+1));
         // It's likely that at this point we will have the other ranks resources
         DLB_UpdateResources();
         #pragma omp parallel for
         for(i=0;i<32;i++) long_task(i*M*(rank+1));
      }
      MPI_Barrier( MPI_COMM_WORLD );
   }

   free(A);
   free(B);
   free(C);
   MPI_Finalize();
   return 0;
}
