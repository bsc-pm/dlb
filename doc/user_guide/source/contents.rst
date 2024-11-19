*****************************
Contents of the installation
*****************************

Structure after installation
============================

.. only:: html

    ::

        dlb/
        ├── bin
        ├── include
        ├── lib
        │   └── cmake
        │       └── DLB
        └── share
            ├── doc
            │   └── dlb
            │       ├── examples
            │       │   ├── DROM
            │       │   ├── MPI+OMP
            │       │   ├── MPI+OMP_OMPT
            │       │   ├── MPI+OMP_roleshift
            │       │   ├── MPI+OmpSs
            │       │   ├── OMPT
            │       │   ├── TALP
            │       │   ├── barrier
            │       │   ├── cmake_project
            │       │   ├── lewi_custom_rt
            │       │   ├── lewi_mask_consumer
            │       │   ├── monitoring_regions
            │       │   ├── named_barrier
            │       │   └── statistics
            │       └── scripts
            ├── man
            │   ├── man1
            │   └── man3
            └── paraver_cfgs
                └── DLB


.. only:: latex

    ::

        dlb/
        |-- bin
        |-- include
        |-- lib
            `-- cmake
            |   `-- DLB
        `-- share
            |-- doc
            |   `-- dlb
            |       |-- examples
            |       |   |-- DROM
            |       |   |-- MPI+OMP
            |       |   |-- MPI+OMP_OMPT
            |       |   |-- MPI+OMP_roleshift
            |       |   |-- MPI+OmpSs
            |       |   |-- OMPT
            |       |   |-- TALP
            |       |   |-- barrier
            |       |   |-- cmake_project
            |       |   |-- lewi_custom_rt
            |       |   |-- lewi_mask_consumer
            |       |   |-- monitoring_regions
            |       |   |-- named_barrier
            |       |   `-- statistics
            |       `-- scripts
            |-- man
            |   |-- man1
            |   `-- man3
            `-- paraver_cfgs
                `-- DLB


Binaries
========

**dlb**
    Basic info, help and version

**dlb_run**
    Run process with DLB pre-initialization, needed to run OMPT applications

**dlb_shm**
    Utility to clean and list an existing shared memory

**dlb_taskset**
    Utility to change the process mask of DLB processes with DROM enabled

Libraries
=========

DLB installs different versions of the library for different situations. The main
ones are:

**libdlb.so**
    Link against this library only if your application needs to call some DLB API function.

**libdlb_mpi.so**
    Preferably, just for preloading using the ``LD_PRELOAD`` environment variable
    if you want DLB to intercept MPI calls.

**libdlb_mpic.so**
    Not installed by default, check :ref:`dlb-mpi-confgure-flags`. Use only if
    ``libdlb_mpi.so`` causes some kind of incompatibility between C and Fortran symbols.

**libdlb_mpif.so**
    Not installed by default, check :ref:`dlb-mpi-confgure-flags`. Use only if
    ``libdlb_mpi.so`` causes some kind of incompatibility between C and Fortran symbols.

There are other libraries that are variations of the above ones for instrumenting or debugging.
These libraries will have the suffixes ``dbg``, ``instr``, or ``instr_dbg``.

.. _distributed_examples:

Examples
========

DLB distributes some examples that are installed in the
``<dlb_install_path>/share/doc/dlb/examples/`` directory. Each example consists of a ``README``
file with a brief description and the steps to follow, a C source code file, a ``Makefile``
to compile the source code and a script ``run.sh`` to run the example.

Some Makefile variables have been filled at configure time. They should
not need any modification but you may check that everything is correct.

.. note::
    In order to enable tracing you need to set ``EXTRAE_HOME`` environment variable
    to a valid Extrae installation.

DROM
----
This example allows you to execute a program with DROM support that prints messages
when its process mask changes. You can run ``dlb_taskset`` while the program is
running and see how it reacts to the different commands.

MPI + OpenMP  /  MPI + OpenMP (OMPT)  /  MPI + OmpSs
----------------------------------------------------
These are different examples with the same structure but different programming
model. The examples use PILS, a synthetic MPI program that can be parameterized
to produce load balance issues between processes. The script ``run.sh`` is
prepared to be modified by the user in order to try different executions and
compare them. These options include enabling DLB, enabling some specific DLB
option, enabling tracing, etc.

LeWI with custom runtime system
-------------------------------
This example shows the integration of a multithreaded runtime system with
LeWI in asynchronous mode.

LeWI Mask with a consumer service
---------------------------------
This example shows the integration of a service that asynchronously requests
the maximum number of CPUs and an MPI application that lends its resources
during an MPI blocking call.

TALP
----
This example shows how a process can attach to DLB and obtain the CPU time on MPI and
the CPU time on useful computation.

Monitoring Regions
------------------
This example shows the usage of the TALP Monitoring Regions, how can they be placed
in a region of the code and obtain some metrics from it.

OMPT
----
This example is a small utility to check whether the application has been linked to
an OpenMP runtime library that supports OMPT.

Barrier
-------
This example shows a coupled application synchronizing with the ``DLB_Barrier``
function.

Named Barrier
-------------
This is another example of synchronisation between a coupled application
running overlapped on the same resources.
This time, each application consumes a result from the other that was generated
in the previous iteration. The synchronisation is done using two named barriers.

CMake project
-------------
This example provides a minimal CMake Project for both Fortran and C code examples.

Statistics
----------
.. note::
    The statistics module has been deprecated and this example is not functional anymore.
    Please contact us if you are interested in using this module.

The last example consists of a PILS program designed to run for a long time, without DLB
micro-load balancing, but with the Statistics module enabled. Check the ``run.sh`` script.
The objective is to let the process run in background while you run one of the other two
binaries provided. These two binaries ``get_pid_list`` and ``get_cpu_usage`` perform basic
queries to the first PILS program and obtain some statistics about CPU usage.

.. _scripts:

Scripts
=======

These scripts are provided for users to simplify the process of enabling some
DLB modules for their applications. These scripts may be copied to a writable
location, modified as required, and run alongside the application as in
``./dlb_script.sh <app> <args>``. Typically, these scripts are correctly
configured and should work out of the box, but it is recommended to double
check the *Run* section at the bottom of the files and that the appropriate DLB
library is configured. See :ref:`examples` for a usage example.

**lewi_omp.sh**
    This script enables the LeWI module on OpenMP applications. It also enables OMPT
    support as long as the OpenMP runtime supports it.

**lewi_omp_trace.sh**
    Same as the previous one, but with Extrae support.

**lewi_ompss.sh**
    This script enables the LeWI module on OmpSs applications.

**lewi_ompss_trace.sh**
    Same as the previous one, but with Extrae support.

**talp.sh**
    This script enables the TALP module. A performance analysis summary will be
    reported at the end of the execution.
