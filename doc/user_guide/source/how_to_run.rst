
.. highlight:: bash

*******************
How to run with DLB
*******************

DLB library is originally designed to be run on applications using a Shared Memory Programming Model
(OpenMP or OmpSs). Although not a hard requirement, since DLB could work with a POSIX threads
application, it is very advisable to use either OpenMP or OmpSs in order to exploit
the thread management of the underlying Programming Model runtime.

Generally, in order to run DLB with your application, you need to follow these steps:

1. Link or preload your application with any flavour of the DLB shared
   libraries: ``libdlb.so`` for simplicity but you can choose other flavours
   like MPI, debug or instrumentation. [#nanos_dlb]_

2. Configure the environment variable ``DLB_ARGS`` with the desired DLB options.
   Typically, you will want to set at least ``--lewi``, ``--drom``, or both.
   Execute ``dlb --help`` for a list of options

3. Run you application as you would normally do.


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
:ref:`non-busy-mpi-calls`.  Just preload the MPI flavour of the DLB library and
enable the option ``--lewi-mpi``::

    $ export DLB_ARGS="--lewi --lewi-mpi"
    $ mpirun -n 2 -x LD_PRELOAD="$DLB_HOME/lib/libdlb_mpi.so" ./foo


MPI + OpenMP
------------
OpenMP is not as malleable as OmpSs since it is still limited by the fork-join
model but DLB can still change the number of threads between parallel regions.
DLB LeWI mode needs to be enabled as before using the environment variable
``DLB_ARGS`` with the value ``--lewi``, and optionally ``--lewi-mpi``.

Running with MPI support here is highly recommended because DLB can lend all
CPUs during a blocking call. Then, we suggest placing calls to ``DLB_Borrow()``
before parallel regions with a high computational load, or at least those near
MPI blocking calls. Take into account that DLB cannot manage the CPU pinning of
each thread and so each MPI rank should run without exclusive CPU binding::

    $ mpicc -fopenmp foo.c -o foo -I"$DLB_HOME/include" \
            -L"$DLB_HOME/lib" -ldlb_mpi -Wl,-rpath,"$DLB_HOME/lib"
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
    $ mpirun -n 2 --bind-to core dlb_run env LD_PRELOAD="$DLB_HOME/lib/libdlb_mpi.so" ./foo

DLB can be fine tuned with the option ``--lewi-ompt``, see section :ref:`ompt`
for more details.


DROM example
============
The DLB DROM module allows to modify the CPUs assigned to an existing process,
and not only the process affinity, also the thread affinity and the number of
active threads to correspond the new assigned mask.

DLB offers an API for third parties to attach to DLB and manage the
computational resources of other DLB running processes. DLB also offers the
``dlb_taskset`` binary, which is basically a wrapper for this API, to manually
configure the assigned CPUs of existing processes. It can also be used to
launch new processes. Run ``dlb_taskset --help`` for further info::

    # Binaries app_1, app_2 and app_3 are assumed to be linked with DLB
    $ export DLB_ARGS="--drom"
    $ ./app_1 &                         # app_1 mask: [0-7]
    $ dlb_taskset --remove 4-7          # app_1 mask: [0-3]
    $ taskset -c 4-7 ./app_2 &          # app_1 mask: [0-3], app_2 mask: [4-7]
    $ dlb_taskset -c 3,7 ./app_3 &      # app_1 mask: [0-2], app_2 mask: [4-6], app_3 mask: [3,7]
    $ dlb_taskset --list


TALP example
============
The TALP module can be used to obtain a summary of some performance metrics
at the end of an execution. For more information about the metrics, visit
the POP metrics website https://pop-coe.eu/node/69::

    $ export DLB_ARGS="--talp --talp-summary=app"
    $ mpirun <options> env LD_PRELOAD="$DLB_HOME/lib/libdlb_mpi.so" ./foo
    DLB[<hostname>:<pid>]: ######### Monitoring Region App Summary #########
    DLB[<hostname>:<pid>]: ### Name:                       MPI Execution
    DLB[<hostname>:<pid>]: ### Elapsed Time :              12.87 s
    DLB[<hostname>:<pid>]: ### Parallel efficiency :       0.70
    DLB[<hostname>:<pid>]: ###   - Communication eff. :    0.91
    DLB[<hostname>:<pid>]: ###   - Load Balance :          0.77
    DLB[<hostname>:<pid>]: ###       - LB_in :             0.79
    DLB[<hostname>:<pid>]: ###       - LB_out:             0.98


**Footnotes**

.. [#nanos_dlb] This step is not needed in OmpSs applications if the Nanos++
    runtime has been configured with DLB support

.. [#mpi_wrapper] These examples are assuming OpenMPI and thus specific variables and
    flags are used, like the variable ``OMPI_CC`` or the flag ``--bind-to``.
    For other MPI implementations, please refer to their documentation manuals.

.. [#ompt_support] At the time of writing only Intel OpenMP and LLVM OpenMP runtimes.

