*******************
Introduction to DLB
*******************

The DLB library will improve the load balance of the outer level of parallelism (e.g MPI) by redistributing the computational resources at the inner level of parallelism (e.g. OpenMP). This readjustment of resources will be done dynamically at runtime.

This dynamism allows DLB to react to different sources of imbalance: Algorithm, data, hardware architecture and resource availability among others.

DLB is composed by three independent modules: Micro Load Balance, Statistics and Dynamic Resource Ownership Manager (DROM).

===========
DLB Modules
===========

Micro Load Balancing
--------------------

.. _statistics:

Statistics
----------
This feature is in development and will be added in version 1.3.

.. _drom:

Dynamic Resource Ownership Manager (DROM)
-----------------------------------------
This feature is in development and will be added in version 1.3.

.. _barrier:

Node Barrier
------------
This feature is in development and will be added in version 1.3.




