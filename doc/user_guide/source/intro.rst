*******************
Introduction to DLB
*******************

The DLB library aims to improve the load balance of HPC hybrid applications (i.e., two levels of parallelism).

In the following picture we can see the structure of a typical HPC hybrid application: one application that will run processes on several nodes, and each process will spawn several threads.

.. image:: images/hpc_app.png
  :width: 300pt
  :align: center
  :alt: Hybrid application

The DLB library will improve the load balance of the outer level of parallelism (e.g MPI) by redistributing the computational resources at the inner level of parallelism (e.g. OpenMP). This readjustment of resources will be done dynamically at runtime.

This dynamism allows DLB to react to different sources of imbalance: Algorithm, data, hardware architecture and resource availability among others.

====================
LeWI: Lend When Idle
====================

The main load balancing algorithm used in DLB is called LeWI (Lend When Idle). The idea of the algorithm is to use the computational resources that are not being used for useful computation to speed up processes in the same computational node.

To achieve this DLB will lend the cpus of a process waiting in a blocking MPI call to another process running in the same node.

.. image:: images/LeWI.png
  :width: 300pt
  :align: center
  :alt: Application balanced with LeWI


.. ===========
.. DLB Modules
.. ===========

.. Micro Load Balancing
.. --------------------

.. The micro load balance module will try to obtain an efficient use of the resources inside the computational node. This module can lend the cpus from one proces to another (different MPI processes of the same application or processes from two different applications).

.. We call it micro load balance because it can react to very fine granularities.

.. .. _statistics:

.. Statistics
.. ----------
.. This feature is in development and will be added in version 1.3.

.. .. _drom:

.. Dynamic Resource Ownership Manager (DROM)
.. -----------------------------------------
.. This feature is in development and will be added in version 1.3.

.. .. _barrier:

.. Node Barrier
.. ------------
.. This feature is in development and will be added in version 1.3.




