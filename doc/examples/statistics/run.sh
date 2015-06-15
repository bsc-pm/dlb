#!/bin/bash

APP="./mpi_ompss_pils"
ARGS="/dev/null 1 100000 50"

export NX_ARGS="--enable-dlb"
export LB_POLICY="No"
export LB_STATISTICS=1

mpirun -n 2 -x LD_PRELOAD=$PRELOAD $APP $ARGS
