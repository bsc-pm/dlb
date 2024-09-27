#!/bin/bash

TRACE=0

if [[ $TRACE == 1 ]] ; then
    export EXTRAE_CONFIG_FILE="extrae.xml"
    preload="$EXTRAE_HOME/lib/libmpitrace.so"
fi

mpirun --use-hwthread-cpus --bind-to hwthread:overload-allowed --map-by :OVERSUBSCRIBE \
    -n 8 env LD_PRELOAD="$preload" ./app 0 : \
    -n 8 env LD_PRELOAD="$preload" ./app 1

mpirun --use-hwthread-cpus --bind-to hwthread:overload-allowed --map-by :OVERSUBSCRIBE \
    -n 8 env LD_PRELOAD="$preload" ./app_barrier 0 : \
    -n 8 env LD_PRELOAD="$preload" ./app_barrier 1
