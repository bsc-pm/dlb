***********************
Contents of the Package
***********************

Structure after installation
============================

.. only:: html

    ::

        dlb/
        ├── bin
        ├── include
        ├── lib
        └── share
            ├── doc
            │   └── dlb
            │       ├── examples
            │       │   ├── DROM
            │       │   ├── MPI+OMP
            │       │   ├── MPI+OMP_OMPT
            │       │   ├── MPI+OmpSs
            │       │   ├── OMPT
            │       │   ├── TALP
            │       │   ├── monitoring_regions
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
        `-- share
            |-- doc
            |   `-- dlb
            |       |-- examples
            |       |   |-- DROM
            |       |   |-- MPI+OMP
            |       |   |-- MPI+OMP_OMPT
            |       |   |-- MPI+OmpSs
            |       |   |-- OMPT
            |       |   |-- TALP
            |       |   |-- monitoring_regions
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

DLB installs different versions of the library for different situations, but in general you
should only focus on these ones:

**libdlb.so**
    Link against this library only if your application needs to call some DLB API function.

**libdlb_mpi.so**
    Link in the correct order or just preload it using ``LD_PRELOAD`` environment variable
    if you want DLB to intercept MPI calls.

**libdlb_mpif.so**
    Same case as above, but to intercept MPI Fortran calls.

Remember that if the programming model already supports DLB (as in Nanos++), you don't need
to link against any library.

Examples
========

DLB distributes some examples that are installed in the
``${DLB_PREFIX}/share/doc/dlb/examples/`` directory. Each example consists of a ``README``
file with a brief description and the steps to follow, a C source code file, a ``Makefile``
to compile the source code and a script ``run.sh`` to run the example.

Some Makefile variables have been filled at configure time. They should should
not need any modification but you may check that everything is correct.  Some
Makefiles assume that Mercurium is configured in the ``PATH``.

.. note::
    In order to enable tracing you need an Extrae installation and to correctly set the
    ``EXTRAE_HOME`` environment variable.

DROM
----
This example allows you to execute a program with DROM support that prints messages
when its process mask changes. You can run ``dlb_taskset`` while the program is
running and see how it reacts to the different commands.

OMPT
----
This example is a small utility to check whether the application has been linked to
an OpenMP runtime library that suports OMPT.

MPI + OpenMP  /  MPI + OpenMP (OMPT)  /  MPI + OmpSs
----------------------------------------------------
These are different examples with the same structure but different programming
model. The examples use PILS, a synthetic MPI program that can be parameterized
to produce load balance issues between processes. The script ``run.sh`` is
prepared to be modified by the user in order to try different executions and
compare them. These options include enabling DLB, enabling some specific DLB
option, enabling tracing, etc.

Monitoring Regions
------------------
This example shows the usage of the TALP Monitorin Regions, how can they be placed
in a region of the code and obtain some metrics from it.

TALP
----
This example shows how a process can attach to DLB and obtain the CPU time on MPI and
the CPU time on useful computation.

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

These scripts are provided for users to simplify the process of enabling some DLB
module for their applications. These scripts should be copied to a write-access location,
modify them if needed and execute them before the application. Typically, these scripts
are correctly configured and should work out of the box, but it is recommended to double
check the *Run* section at the bottom of the files and check whether the appropriate
DLB library is configured. Refer to :ref:`how_to_scripts` for a usage example.

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
