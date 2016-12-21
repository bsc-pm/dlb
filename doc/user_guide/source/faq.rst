*******************************
FAQ: Frequently Asked Questions
*******************************

.. contents::
    :local:

.. philosophical, pre-run requirements

Does my application need to meet any requirements to run with DLB?
==================================================================

Can the number of threads of your application be modified at any time? It is not rare to
manually allocate some private storage for each thread in OpenMP applications and then each
thread in a parallel access this provate storage though the thread id. This kind of methodology
can break the execution if the number os threads is modified and the local storage is not
adjusted, or at least considered.

Mercurium may have DLB support with the ``--dlb`` flag. What does it do? Should I use it?
=========================================================================================

If Mercurium was configured with DLB support, it will accept the ``--dlb`` option flag to
automatically include the DLB headers and link with the correspondig library. If your application
does not use any DLB API function you don't need to use this flag.

Which policy should I use for DLB?
==================================

In some cases each policy should be considered for study, but as a general rule, use
``LB_POLICY=LeWI`` in OpenMP applications and ``LB_POLICY=auto_LeWI_mask`` in OmpSs applications.

.. errors

DLB fails registering a mask. What does it mean?
================================================

When executing your application with DLB you may encounter the following error::

    DLB PANIC[hostname:pid:tid]: Error trying to register CPU mask: [ 0 1 2 3 ]

A process registering into DLB will register its CPU affinity mask as owned CPUs. DLB can move
the ownership of registered CPUs once the execution starts but it will fail with a panic error
if a new process tries to register a CPU already owned by other process.

This typically ocurrs if you run two applications without specifying the process mask, or in
case of MPI, if the ``mpiexec`` is not executed with the right option flags. If the former case
you would need to run the applications using the ``taskset`` command), if the latter every MPI
implementation has different options so you will need to check the appropriate documentation.

.. performance

I'm running a hybrid MPI + OpenMP application but DLB doesn't seem to have any impact
=====================================================================================

Did you place your process and threads in a way they can help each other? DLB aware applications
need to be placed or distributed in a way such that another process in the same node can benefit
from the serial parts of the application.

For instance, in a cluster of 4 CPUs per node you may submit a hybrid job of X MPI process and
4 OpenMP threads per process. That means that each node would only contain one process, so there
will never be resource sharing within the node. Now, if you submit another configuration with
either 2 or 1 threads per process, each node will contain 2 or 4 DLB process that will share
resources when needed.

I'm running a hybrid application, 1 thread per process, and DLB still does nothing
==================================================================================

By default DLB can reduce the number of threads of a process up to a minumum of 1. If the
application is not MPI that's enough because we assume that at least there is always serial
code to do.

But, in MPI applications, we may reach a point where the serial code is only an ``MPI_Barrier``
where the process is only wasting CPU cycles waiting for other processes to synchronize. If
you configure DLB to intercept MPI calls, this CPU can be used instead for helping other
procesess in the same node.

To use this future you need to preload the DLB MPI library and to set this environment variable::

    export LB_MODE=BLOCKING

I'm running a well allocated hybrid MPI + OpenMP but DLB still doesn't do anything.
===================================================================================

There could be several reasons as to why DLB could not help to improve the performance of an
application.

Do you have enough parallel regions to enable the malleability of the number of threads at
different points in your applications?  Try to split you parallel region into smaller parallels.

Is your application very memory bandwith limited? Sometimes increasing the number of threads
in some regions does not increase the performance if the parallel region is already limited by
the memory bandwidth.

Could it be that your application does not suffer from load imbalance? Try our performance tools
to check it out. (http://tools.bsc.es)

.. tracing

Can I see DLB events in a Paraver trace?
========================================

Yes, the effects of DLB are visible in any trace as it involves thread blocking and resuming
but DLB can also emit some events. To do so it will depend on the programming model you want to
trace.

For OmpSs programs: simply compile using the flag ``--instrument`` as you would usually do.

For OpenMP programs: You can link your application with the library ``libdlb_instr.so`` instead.

For MPI programs: Use the DLB version of any Extrae library (ending in -lb.so). If you don't find
the library in the Extrae installation path, reconfigure it using the option
``--with-load-balance=${DLB_PREFIX}``.

Can I disable DLB tracing in an OmpSs execution?
================================================

Yes, simply export the evironment variable ``LB_TRACE_ENABLED=0``.

