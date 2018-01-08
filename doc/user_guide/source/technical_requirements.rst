**********************
Technical Requirements
**********************

==================
Programming models
==================

The currently supported programming models or runtimes by DLB are the following:

* MPI + OpenMP
* MPI + OmpSs
* MPI + SMPSs (not maintained)
* Nanos++ (Multiple Applications)

===============================
Shared Memory between processes
===============================
DLB can balance processes running on the same node and sharing memory. DLB is based on shared memory and needs shared memory between all the processes sharing resources.

.. _mpi-interception:

======================================
Preload mechanism for MPI applications
======================================
When using MPI applications we need the preload mechanism (in general available in any Linux system). This is only necessary when using the MPI interception, it is not necessary when calling DLB with the public API from the application or a runtime.

==========================
Parallel regions in OpenMP
==========================

In OpenMP applications we need parallelism to open and close (parallel region) to be able to change the number of threads. This is because the OpenMP programming model only allows to change the number of threads outside a parallel region.

We also need to add a call to the DLB API to update the resources used before each parallel. This limitation is not present when using the Nanos++ runtime.

==============================
Non-busy waiting for MPI calls
==============================
When using MPI applications we need the MPI blocking calls not to be busy waiting. However, DLB offers a mode where one CPU is reserved to wait for the MPI blocking call and is not lent to other processes.

If the MPI library does not offer a Non-busy waiting mode, or we do not want ot use it of any other reason, We can tell DLB to use a non blocking mode with the option ``--lend-mode=1CPU``.

