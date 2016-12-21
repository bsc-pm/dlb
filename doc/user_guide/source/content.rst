**********************
Content of the Package
**********************

Structure
=========

.. only:: html

    ::

        dlb/
        ├── bin
        ├── include
        ├── lib
        └── share
            ├── doc
            │   └── dlb
            │       └── examples
            │           ├── MPI+OMP
            │           ├── MPI+OmpSs
            │           └── statistics
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
            |       `-- examples
            |           |-- MPI+OMP
            |           |-- MPI+OmpSs
            |           `-- statistics
            `-- paraver_cfgs
                `-- DLB


Binaries
========

**dlb**
    Basic info, help and version

**dlb_shm**
    Utility to manage shared memory

**dlb_taskset**
    Utility to change the process mask of DLB processes

**dlb_cpu_usage**
    Python viewer if using stats

Libraries
=========

DLB installs different versions of the library for different situations, but in general you
should only focus on these ones:

**libdlb.so**
    Link against this library only if your application needs to call some DLB API function.

**libdlb_mpi.so**
    Link in the correct order or just preload it using ``LD_PRELOAD`` environment variable
    if you want DLB to intercetp MPI calls.

**libdlb_mpif.so**
    Same case as above, but to intercep MPI Fortran calls.

Remember that if the programming model already supports DLB (as in Nanos++), you don't need
to link against any library.

Examples
========

Currently three examples are distributed with the source code and installed in the
``${DLB_PREFIX}/share/doc/dlb/examples/`` directory. Each example consists of a ``README``
file with a brief description and the steps to follow, a C source code file, a ``Makefile``
to compile the source code and a script ``run.sh`` to run the example.

Some Makefile variables have been filled at configure time. They should be enough to compile
and link the examples with MPI support. Some Makefiles assume that Mercurium is configured
in the ``PATH``.

.. note::
    In order to enable tracing you need an Extrae installation and to correctly set the
    ``EXTRAE_HOME`` environment variable.

.. note::
    Some examples preload the DLB Extrae library (ending in trace-lb.so). If you
    don't have those libraries installed reconfigure your Extrae installation with the
    option ``--with-load-balancing=${DLB_PREFIX}``

MPI + OpenMP
------------
PILS is a synthetic MPI program with some predefined load balancing issues. Simply check
the ``Makefile`` if everything is correct and run ``make``. The ``run.sh`` script should
also contain the MPI detected at configure time. Apart from that, two options can be modified
at the top of the file, whether you want to enable *DLB* or to enable *TRACE* mode (or both).

A very similar example but just using OpenMP. Notable differences are the ``-fopenmp`` flag
used in the ``Makefile`` that assumes a GNU-like flag. The ``run.sh`` script is also
configured to allow two options, *DLB* and *TRACE*.

MPI + OmpSs
-----------
A very similar example but just using OmpSs. Make sure that Mercurium is in your ``PATH``
or modify the Makefile accordingly. Then, you can run it in the same way as the previous
example.

Statistics
----------
The last example consists of a PILS program designed to run for a long time, without DLB
micro-load balancing, but with the Statistics module enabled. Check the ``run.sh`` script.
The objective is to let the process run in background while you run one of the other two
binaries provided. These two binaries ``get_pid_list`` and ``get_cpu_usage`` perform basic
queries to the first PILS program and obtain some statistics about CPU usage.
