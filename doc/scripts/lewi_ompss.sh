#!/usr/bin/env bash

# Description:
#   Execute an OmpSs application with the dynamic load policy LeWI (Lend When Idle).
#
# Usage:
#   ./lewi_ompss.sh <application> <args>

#################################################################################
### DLB options                                                               ###
#################################################################################

# Enable LeWI
export DLB_ARGS="$DLB_ARGS --lewi"

# Keep one CPU during a blocking call
#export DLB_ARGS="$DLB_ARGS --lewi-keep-one-cpu"

# Select which MPI calls to use for LewI
# Possible values: [none, all, barrier, collectives], default: all
#export DLB_ARGS="$DLB_ARGS --lewi-mpi-calls=all"

# Select affinity policy to prioritize CPUs to acquire or borrow during LeWI
# Possible values: [any, nearby-first, nearby-only, spread-ifempty],
# default: nearby-first
#export DLB_ARGS="$DLB_ARGS --lewi-affinity=nearby-first"


#################################################################################
### Nanos++ options                                                           ###
#################################################################################

# Enable DLB and thread block on idle
export NX_ARGS="$NX_ARGS --enable-dlb --enable-block"

# Force user code to run on the primary thread. It can alleviate MPI issues.
#export NX_ARGS="$NX_ARGS --force-tie-master"


#################################################################################
### Run                                                                       ###
#################################################################################

"$@"
