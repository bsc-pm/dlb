*******************
How to run with DLB
*******************

DLB library is originally designed to be run on applications using a Shared Memory Programming Model
(OpenMP or OmpSs), although it is not a hard requirement, it is very advisable in order to exploit
the thread management of the underlying Programming Model runtime.

Examples by Programming Model
=============================

.. highlight:: bash

OmpSs
-----
If you are running an OmpSs application you just need to set two parameters. First, append
``--thread-manager=dlb`` to the Nanos++ ``NX_ARGS`` environment variable. With this option,
the Nanos++ runtime relies on DLB to take every decision about thread management. Second,
set the DLB variable ``LB_POLICY="auto_LeWI_mask"``, which is the LeWI policy for autonomous
threads. This DLB policy needs a highly malleable runtime, as Nanos++ is.

You don't need to do any extra steps if your application doesn't use the DLB API, as all the
communication with DLB is handled by the Nanos++ runtime. Otherwise, just add the flag
``--dlb`` to the Mercurium compiler to automatically add the required compile and link flags::

    $ smpfc --ompss [--dlb] foo.c -o foo
    $ export NX_ARGS+=" --thread-manager=dlb"
    $ export LB_POLICY="auto_LeWI_mask"
    $ ./foo


MPI + OmpSs
-----------
In the same way, you can run MPI + OmpSs applications with DLB::

    $ OMPI_CC="smpfc --ompss [--dlb]" mpicc foo.c -o foo
    $ export NX_ARGS="--thread-manager=dlb"
    $ export LB_POLICY="auto_LeWI_mask"
    $ mpirun -n 2 ./foo

However, MPI applications with only one running thread may become blocked due to the main
thread entering a synchronization MPI blocking point. DLB library can intercept MPI calls
to overload the CPU in these cases::

    $ export LB_LEND_MODE="BLOCK"
    $ mpirun -n 2 -x LD_PRELOAD=${DLB_PREFIX}/lib/libdlb_mpi.so ./foo

OpenMP
------
OpenMP is not as malleable as OmpSs so the ideal DLB policy is the simple ``LeWI``, without
individual thread management nor CPU mask support. Also, unless you are using Nanos++  as an
OpenMP runtime, you will need to manually call the API functions in your code, thus you will
need to pass the compile and linker flags to your compiler::

    $ gcc -fopenmp foo.c -o foo -I${DLB_PREFIX}/include \
            -L${DLB_PREFIX}/lib -ldlb -Wl,-rpath,${DLB_PREFIX}/lib
    $ export LB_POLICY="LeWI"
    $ ./foo

MPI + OpenMP
------------
In the case of MPI + OpenMP applications, you can let all the DLB management to the MPI
interception. Thus, allowing you to run DLB applications without modifying your binary.
Simply, preload an MPI version of the library and DLB will balance the resource of each
process during the MPI blocking calls::

    $ mpicc -fopenmp foo.c -o foo
    $ export LB_POLICY="LeWI"
    $ mpirun -n 2 -x LD_PRELOAD=${DLB_PREFIX}/lib/libdlb_mpi.so ./foo

