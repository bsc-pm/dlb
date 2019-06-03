#!/bin/bash

export OMP_WAIT_POLICY="passive"
export DLB_ARGS="--ompt --verbose=ompt"
./app
