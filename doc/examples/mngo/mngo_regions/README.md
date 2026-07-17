
# Test MNGO with regions

This is an example of how to use DLB MNGO with regions. In this mode mngo will use code annotations
to measure the performance of specific regions of the code and apply different load balancing
polices.

MNGO will use either LeWI or DROM depending on what is enabled at runtime in the DLB_ARGS.
Currently if both LeWI and DROM are enabled DROM will take preference.

## Build example

In this folder you can find the same example implemented both in Frotran and in C, at this step you
can choose either version.

For this test to work you need DLB installed with MPI support, and to set `DLB_HOME` to the
corresponding installation path.

To build the C version:
```
mpicc -o test_regions test_regions.c \
  -I${DLB_HOME}/include -L${DLB_HOME}/lib -Wl,-rpath,${DLB_HOME}/lib -ldlb -fopenmp
```

To build the Fortran version:
```
mpifort -o test_regions test_regions.f90 \
  -I${DLB_HOME}/include -L${DLB_HOME}/lib -Wl,-rpath,${DLB_HOME}/lib -ldlb -fopenmp
```

## Run

To execute the test you will need to configure the `DLB_ARGS` and set the `LD_PRELOAD`. Since MNGO
requires TALP it will be enabled by default.

For example to run DLB MNGO with DROM you could configure DLB like this:
```
export DLB_ARGS="--mngo --drom"
```

You can read the `run.regions.sh` to see different ways you can run this test.
