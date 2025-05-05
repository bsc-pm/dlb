#!/usr/bin/env bash

export OMP_NUM_THREADS=1
export DLB_ARGS="--ompt --verbose=ompt"
./app
