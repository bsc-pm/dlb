#!/bin/bash

mpirun --use-hwthread-cpus --bind-to hwthread:overload-allowed --map-by :OVERSUBSCRIBE \
    -np 8 ./test_first : \
    -np 8 ./test_second
