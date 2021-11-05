
.. highlight:: bash

*******************************
FAQ: Frequently Asked Questions
*******************************

.. contents::
    :local:

.. philosophical, pre-run requirements

Does my application need to meet any requirements to run with DLB?
==================================================================

The only limitation from the application standpoint is that it must allow
changing the number of threads at any time.

Some common pattern in OpenMP applications is to allocate private thread
storage for intermediate results between different parallel constructs. This
technique may break the execution if different parallel regions are executed
with different number of threads.

If this limitation cannot be solved, you can use the DLB API to only change the
number of threads on code regions that are safe to do so.

Which should I use, LeWI or DROM for my application?
====================================================

LeWI and DROM modules serve different purposes. Their use is completely independent from
each other so you can enable one of them or both.

Use LeWI (``DLB_ARGS+=" --lewi"``) to enable dynamic resource sharing between processes
due to load imbalance. Use DROM (``DLB_ARGS+=" --drom"``) to enable on-demand resource
management.

.. errors

DLB fails registering a mask. What does it mean?
================================================

When executing your application with DLB you may encounter the following error::

    DLB PANIC[hostname:pid:tid]: Error trying to register CPU mask: [ 0 1 2 3 ]

A process registering into DLB will register its CPU affinity mask as owned CPUs. DLB can move
the ownership of registered CPUs once the execution starts but it will fail with a panic error
if a new process tries to register a CPU already owned by other process.

This typically occurs if you run two applications without specifying the process mask, or in
case of MPI applications, if the ``mpiexec`` command was executed without the bindings
flag options. In the former case you would need to run the applications using the
``taskset`` command, if the latter case every MPI implementation has different options so you
will need to check the appropriate documentation.

.. performance

I'm running a hybrid MPI + OpenMP application but DLB doesn't seem to have any impact
=====================================================================================

Did you place your process and threads in a way they can help each other? DLB aware applications
need to be placed or distributed in a way such that another process in the same node can benefit
from the serial parts of the application.

For instance, in a cluster of 4 CPUs per node you may submit a hybrid job of *n* MPI processes and
4 OpenMP threads per process. That means that each node would only contain one process, so there
will never be resource sharing within the node. Now, if you submit another distribution with
either 2 or 1 threads per process, each node will contain 2 or 4 DLB processes that will share
resources when needed.

I'm running a well allocated hybrid MPI + OpenMP but DLB still doesn't do anything.
===================================================================================

There could be several reasons as to why DLB could not help to improve the performance of an
application.

Do you have enough parallel regions to enable the malleability of the number of threads at
different points in your applications?  Try to split you parallel region into smaller parallels.

Is your application very memory bandwidth limited? Sometimes increasing the number of threads
in some regions does not increase the performance if the parallel region is already limited by
the memory bandwidth.

Could it be that your application does not suffer from load imbalance? Try our performance tools
to check it out. (http://tools.bsc.es)

How do I share CPUs between unrelated applications?
===================================================
Even if the applications are not related and started at different time, they can share CPUs
as if they were an MPI application with multiple processes.

Do note, however, that as soon as one of them finishes, all CPUs that belonged
to it will be removed from the DLB shared memory and they won't be accessible
anymore by other processes. This can be avoided by setting
``DLB_ARGS+=" --debug-opts=lend-post-mortem``.

How do I prevent sharing CPUs between applications?
===================================================
On the other hand, you may also be interested in avoiding DLB resource sharing
for some applications. For instance, running applications *A* and *B* and
sharing CPUs only between them, and at the same time running applications *C* and *D*
and sharing CPUs also only between them. This can be done by setting different shared
memories for each subset of applications with the option ``--shm-key``::

    $ export DLB_ARGS="--lewi --shm-key=AB"
    $ ./A &
    $ ./B &
    $ export DLB_ARGS="--lewi --shm-key=CD"
    $ ./C &
    $ ./D &


.. tracing

Can I see DLB in action in a Paraver trace?
===========================================

Yes, DLB actions are clearly visible in a Paraver trace as it involves thread blocking and
resuming. Trace your application as you would normally do using the Extrae library that
matches your programming model.

Can I see DLB events in a Paraver trace?
========================================

Yes, DLB can emit tracing events for debugging or advanced purposes, just use the appropriate
DLB library. Apart from tracing as you would normally do, you need to either link your application
or preload with one of the libray flavours for instrumentation. These are ``libdlb_instr.so``,
``libdlb_mpi_instr.so`` or ``libdlb_mpif_instr.so``.

You can find predefined Paraver configurations in the installation directory
``$DLB_PREFIX/hare/paraver_cfgs/DLB/``.

What DLB library do I need to use if I want to trace the application but not DLB?
=================================================================================

Short answer: the same as if you were tracing DLB but with ``DLB_ARGS+=" --instrument=none"``.
If your application has MPI, DLB still has to be aware of MPI and yet it needs to avoid the MPI
symbols interception. This is what the libraries ``libdlb_mpi_instr.so`` and
``libdlb_mpif_instr.so`` do, only Extrae will intercept MPI and will forward that information
to DLB.
