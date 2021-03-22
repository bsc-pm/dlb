**********************
Technical Requirements
**********************
This is a list of technical requirements that the DLB user should be aware of.
Not all all of them are strictly necessary and will depend on the application
or the programming model used, but it is important to know all these factors
for a successful DLB execution.

==================
Programming models
==================
DLB was original designed to mitigate load imbalance issues in HPC applications
that used two programming models simultaneously: MPI for the outer level of
parallelism, and any thread-based programming model for the inner level, such
as OpenMP or OmpSs.

But, technically, none of them are strictly necessary. One application could
have its own thread management and it could work with DLB as long as the
application used DLB for the resource sharing. And, MPI is not necessary either
since you can run two different non-MPI applications with DLB and share CPUs
between them.

Although, the scenario presented above requires a considerable amount of effort
and we strongly recommend to use applications MPI + OpenMP/OmpSs.

===================
POSIX Shared Memory
===================
DLB is able to fix load balance issues only among processes on the same node.
This is basically because the library only adapts the number of CPUs assigned
to each process and does not perform any data balance, through MPI for
instance.

Since the communication needed in DLB is only at node level, the library uses
POSIX shared memory objects that should be available at any GNU/Linux system.

.. _mpi-interception:

======================================
Preload mechanism for MPI applications
======================================
DLB implements the MPI interface to capture each MPI call made by the
application, and then calls the original MPI function by using the PMPI
profiling interface.

To enable MPI support for DLB, the application must be either linked or
preloaded with one of the DLB MPI libraries, e.g., ``libdlb_mpi*.so``.  Using
the preload mechanism has the advantage that the application binary is the
same as the original, and it only requires setting an environment variable
before the execution.

Use the environment variable ``LD_PRELOAD`` to specify the MPI DLB library.

.. _non-busy-mpi-calls:

==============================
Non-busy-waiting for MPI calls
==============================
Usually, MPI implementations have at least two ways of waiting for a blocking
MPI call: *busy-waiting* and *non-busy-waiting*; or even a hybrid parameterized
mode.  In *busy-waiting* mode, the thread that calls the MPI function
repeatedly checks if the MPI call is finished, while in *non-busy-waiting* mode
the thread may use other techniques to block its execution and not overuse the
current CPU.

DLB lends all CPUs by default. That means that even if a thread is inside
MPI in *busy-waiting* mode, that CPU can be assigned to other process. If you
want to keep the CPU when an MPI blocking function is invoked, use the option
``--lewi-keep-one-cpu``.

==========================
Parallel regions in OpenMP
==========================
OpenMP parallelism is based on a fork-join model, where threads are created, or
at least assigned to a team, only when the running thread encounters a parallel
construct.

This model limits DLB because it can only assign CPUs to a process just before
a parallel construct. For this reason, an application with very few parallel
constructs may take minimal advantage from DLB.
