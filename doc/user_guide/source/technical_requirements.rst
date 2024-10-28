**********************
Technical Requirements
**********************
This is a list of technical requirements that the DLB user should be aware of.
Not all of these are strictly necessary and will depend on the application
or programming model used, but it is important to know all these factors
for a successful DLB execution.

==================
Programming models
==================
DLB was originally designed to alleviate load imbalance issues in HPC applications
using two programming models simultaneously: MPI for the outer level of
parallelism, and any thread-based programming model for the inner level, such
as OpenMP or OmpSs.

But technically, none of the above is strictly necessary. An application could
have its own thread management and still use DLB, as long as the application
communicates with the library to share resources dynamically. MPI is not
necessary either, DLB supports resource sharing between different processes,
even if they are not the same application (or they are, but DLB is not aware of
it).  Nevertheless, DLB uses MPI to detect which process is more unbalanced,
and without this information, the application has to provide it to DLB via its
API.

===================
POSIX Shared Memory
===================
DLB can only solve load balancing problems between processes in the same node.
This is basically because the library only adjusts the number of CPUs assigned
to each process and does not perform any data balancing, like moving
data via MPI.

Since the communication needed in DLB is only at node level, the library uses
POSIX shared memory objects, which should be available on any GNU/Linux system.

.. _mpi-interception:

======================================
Preload mechanism for MPI applications
======================================
DLB implements the MPI interface to capture each MPI call made by the
application, and then calls the original MPI function using the PMPI
profiling interface.

To enable MPI support for DLB, the application must either be linked or
preloaded with the DLB MPI library ``libdlb_mpi.so``.  Using
the preload mechanism has the advantage that the application binary is the
same as the original, and it only requires setting an environment variable
before execution.

Use the ``LD_PRELOAD`` environment variable to specify the MPI DLB library.

The library ``libdlb_mpi.so`` should intercept both C and Fortran MPI symbols.
In case of problems, check :ref:`DLB configure flags<dlb-configure-flags>` to
build separate libraries.

.. _non-busy-mpi-calls:

==============================
Non-busy-waiting for MPI calls
==============================
Typically, MPI implementations have at least two ways of waiting for a blocking
MPI call: *busy-waiting* and *non-busy-waiting*; or even a parameterised hybrid
mode.  In *busy-waiting* mode, the thread calling the MPI function
repeatedly checks if the MPI call is finished, while in *non-busy-waiting* mode
the thread can use other techniques to block its execution and not overuse the
current CPU.

By default, DLB shares all CPUs whose associated thread is blocked in MPI.
This means that even if a thread is in MPI *busy-waiting* mode, this CPU can be
assigned to another process. To avoid sharing the CPU while it is executing an
MPI blocking call, use the ``--lewi-keep-one-cpu`` option.

==========================
Parallel regions in OpenMP
==========================
OpenMP (5.2 and below) parallelism is based on a fork-join model where threads
are created (virtually or assigned to a team) when the running thread
encounters a parallel construct.

This model limits DLB because it can only assign CPUs to a process just before
a parallel construct. For this reason, an application with very few parallel
constructs may get minimal benefit from DLB.

This limitation may be overcome with the upcoming OpenMP 6.0 and its free-agent
threads, where tasks can be assigned to threads that may be created later and
not part of a parallel region.
