********
Examples
********

Currently three examples are distributed with the source code, which are installed in the
``${DLB_PREFIX}/share/doc/dlb/examples/`` directory. Each example consists of a ``README``
file with a brief description and the steps to follow, a C source code file, a ``Makefile``
to compile the source code and a script ``run.sh`` to run the example. The only prerequisite
is to check the Makefile dependencies, like an MPI wrapper or the Mercurium compiler for
the examples in OmpSs.

MPI + OmpSs
===========
PILS is a synthetic MPI program with some predefined load balancing issues. Simply check
the ``Makefile`` and modify it accordingly if you are using other MPI implementation than
OpenMPI. Use the ``I_MPI_CC`` environment variable instead, or check your MPI documentation.
the ``run-sh`` script is also written for an OpenMPI mpirun command. Apart from that, two
options can be modified at the top of the file, whether you want to enable *DLB* or to enable
*TRACE* mode (or both).

.. note::
    In order to enable tracing you need an Extrae installation and to correctly set the
    ``EXTRAE_HOME`` environment variable.

MPI + OpenMP
============
A very similar example but just using OpenMP. Notable differences are the ``-fopenmp`` flag
used in the ``Makefile`` that assumes a GNU-like flag. The ``run.sh`` script is also
configured to allow two options, *DLB* and *TRACE*.

.. note::
    The Extrae library used in this example when both options *DLB* and *TRACE* are enabled
    is a modified version which calls DLB hooks when an MPI call is captured. If you don't
    have this version of the library (ending in trace-lb.so), reconfigure your Extrae
    installation with the option ``--with-load-balancing=${DLB_PREFIX}``

Statistics
==========
The last example consists of a PILS program designed to run for a long time, without DLB
micro-load balancing, but with the Statistics module enabled. Check the ``run.sh`` script.
The objective is to let the process run in background while you run one of the other two
binaries provided. These two binaries ``get_pid_list`` and ``get_cpu_usage`` perform basic
queries to the first PILS program and obtain some statistics about CPU usage.
