#!/usr/bin/env sh

patch pils.c monitoring_regions/mpi_pils.c.patch    -o monitoring_regions/mpi_pils.c
patch pils.c mpi+omp/mpi_omp_pils.c.patch           -o mpi+omp/mpi_omp_pils.c
patch pils.c mpi+omp_ompt/mpi_omp_pils.c.patch      -o mpi+omp_ompt/mpi_omp_pils.c
patch pils.c mpi+omp_roleshift/mpi_omp_pils.c.patch -o mpi+omp_roleshift/mpi_omp_pils.c
patch pils.c mpi+ompss/mpi_ompss_pils.c.patch       -o mpi+ompss/mpi_ompss_pils.c
patch pils.c statistics/mpi_ompss_pils.c.patch      -o statistics/mpi_ompss_pils.c
