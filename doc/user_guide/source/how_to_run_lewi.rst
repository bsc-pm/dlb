
.. highlight:: bash

***********************************************
LeWI for improving Load Balance in Applications
***********************************************

The LeWI module is used in hybrid MPI+OpenMP/OmpSs applications to dynamically
and transparently change the process number of threads and their CPU affinity.

By intercepting MPI calls, DLB is able to detect when the process reaches a
state where the worker threads are idle, then it may temporarily yield the
process assigned CPUs to another process that would benefit from them.

Depending on the Programming Model used and the runtime, in case of OpenMP,
DLB may be used automatically, or sometimes with a few modifications in the
source code.

.. _how_to_scripts:

Using a DLB script provided in the installation
===============================================
The DLB installation provides some scripts to run applications easily with DLB support.
There are different scripts to enable LeWI with a specific programming model, either
OpenMP or OmpSs.

These scripts contain common DLB options for that case and a description of other
options that may be of interest. To use these scripts, the recommended method is to
copy the script you want to use, review it or modify it if needed, and then run the
script just before the application::

    $ cp $DLB_PREFIX/share/doc/dlb/scripts/lewi_omp.sh .
    # Optionally, review and edit lewi_omp.sh
    $ mpirun <options> ./lewi_omp.sh ./foo

Refer to :ref:`scripts` for more information.

LeWI Examples by Programming Model
==================================
For simplicity, all the LeWI examples are MPI applications. [#mpi_wrapper]_

MPI + OmpSs
-----------
OmpSs applications have the advantage they do not need to be explicitly linked
with DLB, since Nanos++, the OmpSs runtime, has native DLB support that can be
enabled at configure time.

First, compile your application setting Mercurium as the native compiler for
the MPI wrapper and, optionally, use the flag ``--dlb`` to automatically add
the DLB compile and link flags in case you use the DLB API.

Then, enable DLB support in Nanos++ by setting the environment variable
``NX_ARGS="--enable-dlb --enable-block"``, and enable also LeWI in DLB with
``DLB_ARGS="--lewi"``::


    $ OMPI_CC="smpcc --ompss [--dlb]" mpicc foo.c -o foo
    $ export NX_ARGS="--enable-dlb --enable-block"
    $ export DLB_ARGS="--lewi"
    $ mpirun -n 2 ./foo

You may also enable MPI support for DLB after considering
:ref:`non-busy-mpi-calls`. To do so, either link or preload the MPI flavour of
the DLB library.  If you find that the MPI blocking calls are busy waiting,
consider using the option ``--lewi-keep-one-cpu`` to keep the CPU that is doing
the blocking call for the current process::

    $ export DLB_ARGS="--lewi"
    $ mpirun -n 2 -x LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo



MPI + OpenMP
------------
OpenMP is not as malleable as OmpSs since it is still limited by the fork-join
model but DLB can still change the number of threads between parallel regions.
DLB LeWI mode needs to be enabled as before using the environment variable
``DLB_ARGS`` with the value ``--lewi``, and optionally ``--lewi-keep-one-cpu``.

Running with MPI support here is highly recommended because DLB can lend all
CPUs during a blocking call. Then, we suggest placing calls to ``DLB_Borrow()``
before parallel regions with a high computational load, or at least those near
MPI blocking calls. Take into account that DLB cannot manage the CPU pinning of
each thread and so each MPI rank should run without exclusive CPU binding::

    $ mpicc -fopenmp foo.c -o foo -I"$DLB_PREFIX/include" \
            -L"$DLB_PREFIX/lib" -ldlb_mpi -Wl,-rpath,"$DLB_PREFIX/lib"
    $ export DLB_ARGS="--lewi"
    $ mpirun -n 2 --bind-to none ./foo


MPI + OpenMP (with OMPT support)
--------------------------------
OpenMP 5.0 implements a new interface for Tools (OMPT) that allows external
libraries, in this case DLB, to track the runtime state and to register
callbacks for defined OpenMP events. If your OpenMP runtime supports it
[#ompt_support]_, DLB can automatically intercept parallel constructs and
modify the number of threads at that time, without modifying the application
source code.

Note than DLB with OMPT support can manage the CPU pinning of each thread so
each rank must run with an exclusive set of CPUs::


    $ OMPI_CC=clang mpicc -fopenmp foo.c -o foo
    $ export DLB_ARGS="--lewi --ompt --lewi-ompt=borrow:lend"
    $ mpirun -n 2 --bind-to core dlb_run ./foo

Since this example does not need to be linked with DLB, you will need to
preload a DLB MPI library if you want MPI support::

    $ export DLB_ARGS="--lewi --ompt --lewi-ompt=borrow:mpi"
    $ mpirun -n 2 --bind-to core dlb_run env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

DLB can be fine tuned with the option ``--lewi-ompt``, see section :ref:`ompt`
for more details.


**Footnotes**

.. [#mpi_wrapper] These examples are assuming OpenMPI and thus specific variables and
    flags are used, like the variable ``OMPI_CC`` or the flag ``--bind-to``.
    For other MPI implementations, please refer to their documentation manuals.

.. [#ompt_support] At the time of writing only Intel OpenMP and LLVM OpenMP runtimes.

