#!/usr/bin/env sh

diff pils.c monitoring_regions/mpi_pils.c       > monitoring_regions/mpi_pils.c.patch
diff pils.c mpi+omp/mpi_omp_pils.c              > mpi+omp/mpi_omp_pils.c.patch
diff pils.c mpi+omp_ompt/mpi_omp_pils.c         > mpi+omp_ompt/mpi_omp_pils.c.patch
diff pils.c mpi+omp_roleshift/mpi_omp_pils.c    > mpi+omp_roleshift/mpi_omp_pils.c.patch
diff pils.c mpi+ompss/mpi_ompss_pils.c          > mpi+ompss/mpi_ompss_pils.c.patch
diff pils.c statistics/mpi_ompss_pils.c         > statistics/mpi_ompss_pils.c.patch
