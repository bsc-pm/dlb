/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

/*<testinfo>
    compile_versions="nanox_ompss"

    test_CC_nanox_ompss="env OMPI_CC=smpcc mpicc"
    test_CFLAGS_nanox_ompss="--ompss"

    test_generator="gens/mpi-generator"
</testinfo>*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <sched.h>
#include <utmpx.h>
#include <omp.h>
#include "LB_core/DLB_interface.h"

#define N 8192
#define M 256

int size, rank;
double *A, *B, *C;

void compute(int n) {
    int i;
    for (i=n; i<n+M; i++) {
        A[i] = i*i;
        B[i] = i*7;
    }

    for (i=n; i<n+M; i++) {
        C[i] = A[i] / B[i];
    }
}

#pragma omp task
void short_task(int n) {
    int i;
    for (i=0; i<N; i++) { compute(n); }
}

#pragma omp task
void long_task(int n) {
    int i;
    for (i=0; i<2*N; i++) { compute(n); }
}

int main(int argc, char* argv[]) {
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
            for(i=0; i<16; i++) { short_task(i*M*(rank+1)); }
        } else {
            for(i=0; i<32; i++) { long_task(i*M*(rank+1)); }
        }
        #pragma omp taskwait
        MPI_Barrier( MPI_COMM_WORLD );
    }

    free(A);
    free(B);
    free(C);
    MPI_Finalize();
    return 0;
}
