#!/bin/bash

# variables to be modified by the user
TRACE=1
DLB=0

APP="./mpi_ompss_pils"
ARGS="/dev/null 1 150 50"

if [[ $TRACE == 1 ]] ; then
    PRELOAD="$EXTRAE_HOME/lib/libnanosmpitrace.so"
    export EXTRAE_CONFIG_FILE="extrae.xml"
    export NX_ARGS+=" --instrumentation=extrae"
    if [[ $DLB == 1 ]] ; then
        export TRACENAME="pils_dlb.prv"
    else
        export TRACENAME="pils.prv"
    fi
fi

if [[ $DLB == 1 ]] ; then
    export NX_ARGS+=" --thread-manager=dlb"
    export LB_POLICY="auto_LeWI_mask"
else
    export LB_POLICY="No"
fi

mpirun -n 2 -x LD_PRELOAD=$PRELOAD $APP $ARGS
