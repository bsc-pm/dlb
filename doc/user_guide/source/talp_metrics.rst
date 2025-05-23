
.. highlight:: bash
.. _talp_metrics:

******************************************************
TALP Performance metrics
******************************************************


The performance metrics reported by TALP are derived similarly as the `POP metrics <https://pop-coe.eu/node/69>`_.

The figure below shows the hierarchy of metrics which categorises efficiency-losses into programming-model specific categories.


.. image:: images/talp-efficiency-models.*


TALP is able to generate these metrics for :ref:`every region that is added.<talp-custom-regions>`

Below, we will briefly explain how the metrics are computed.

For simplicity, we assume that all MPI processes run with the same number of
OpenMP threads, and that all OpenMP threads execute the same parallel regions.

.. note::
    TALP is actually able to handle applications where these conditions don't hold, but we chose to keep the derivations as clear as possible.


How the metrics are computed 
==============================

First we introduce some common notion which we use to classify the execution time. 
Let :math:`N` be the number of MPI processes, :math:`M` be the number of OpenMP threads per process and :math:`P:` the number of parallel regions of each thread.

Classification of execution time
--------------------------------

With this we can define :math:`T_{i,j}` to be the total time spent in the :math:`j`-th OpenMP thread of the :math:`i`-th MPI process.
We classify time spent by threads in three categories:

* :math:`U`, doing useful computation
* :math:`C`, MPI communication
* :math:`I`, idle OpenMP threads and time spent within the OpenMP runtime.

Let :math:`T^{k}_{i,j}, \forall k \in \{U, C, I \}` be the time spent in the
:math:`k`-th category for the given thread and process.

Therefore,

.. math::
    T_{i,j} = T^{U}_{i,j} + T^{C}_{i,j} + T^{I}_{i,j}

Let :math:`T^k_{i}` be the average thread time per process such that:

.. math::
    T^{k}_{i} = \frac{\sum_{j}^{M} T_{i,j}^{k}}{M},~
    \forall k \in \{U, C, I \}

We further subdivide the time spent by idle threads (:math:`T^I`) into three subcategories:

* :math:`I_\mathrm{serial}`, outside parallel regions;
* :math:`I_\mathrm{lb}`, waiting for the slowest thread of each parallel region; and
* :math:`I_\mathrm{sch}`, all other time spent idling inside parallel regions.

Let :math:`T^I_{i,j,p}` be the idle time inside the :math:`p`-th OpenMP parallel region of the :math:`j`-th thread of the :math:`i`-th process.

Let :math:`T^{I_\mathrm{serial}}_{i}` be the average thread time spent by process :math:`i` outside OpenMP,

.. math::
    T^{I_\mathrm{serial}}_{i} = \frac{\sum_{j \in [1,M]} (T^{I}_{i,j} - \sum_{p \in [1,P]} T^I_{i,j,p})}{N}

We define :math:`T^{I_{lb}}_{i}` to be the average thread time spent by process :math:`i` waiting for thread synchronisation due to load imbalance,

.. math::
    T^{I_{lb}}_{i} = \sum_{p \in [1,P]}( T^I_{i,p} - \min_{j \in [1,M]} T^I_{i,j,p} )

:math:`T^{I_\mathrm{sch}}_{i}` is the average thread time spent by process :math:`i` lost due to scheduling,

.. math::
    T^{I_\mathrm{sch}}_{i} = \sum_{p \in [1,P]} \min_{j \in [1,M]} T^I_{i,j,p}

:math:`T^k` be the average process execution time such that:

.. math::
    T^{k} = \frac{\sum_{i}^{N} T_{i}^{k}}{N},~
    \forall k \in \{U, C, I_\mathrm{serial}, I_{lb}, I_{sch} \}

OpenMP metrics
--------------

Let :math:`\mathrm{OMP}_\mathrm{Serial}` (*OpenMP Serialization Efficiency*) represent the time lost
because OpenMP was not running a parallel region.

.. math::
    \mathrm{OMP}_{Serial} = (T - T^{I_{serial}}) / T

This metric is used to account for regions not parallelized with OpenMP.

Let :math:`\mathrm{OMP}_\mathrm{LB}` (*OpenMP Load Balance Efficiency*) represent the time lost
in idle OpenMP threads within a parallel region due to an uneven distribution
among its threads.

.. math::
    \mathrm{OMP}_\mathrm{LB} = (T - T^{I_\mathrm{serial}} - T^{I_{lb}}) / (T - T^{I_\mathrm{serial}})

Let :math:`\mathrm{OMP}_\mathrm{Sched}` (*OpenMP Scheduling Efficiency*) represent the time lost in idle
OpenMP threads within a parallel region not caused by load imbalance.

.. math::
    \mathrm{OMP}_\mathrm{Sched} = (T - T^{I_\mathrm{serial}} - T^{I_{lb}} - T^{I_\mathrm{sch}}) / (T - T^{I_\mathrm{serial}} - T^{I_{lb}})

Let :math:`\mathrm{OMP}_\mathrm{Eff}` (*OpenMP Parallel Efficiency*) be the efficiency
considering only time when OpenMP threads are idle as lost,

.. math::
    \mathrm{OMP}_\mathrm{Eff} = (T - T^I) / T = \mathrm{OMP}_\mathrm{Serial} \times \mathrm{OMP}_\mathrm{LB} \times \mathrm{OMP}_\mathrm{Sched}

MPI metrics
---------------------------

In the hybrid model, the MPI metrics are redefined as:

.. math::
    \mathrm{MPI}_\mathrm{Eff} = (T - T^M) / T

.. math::
    \mathrm{MPI}_\mathrm{LB} = (T - T^M) / (max_{i\in[1,N]} T_i - T_{i}^{M})

.. math::
    \mathrm{MPI}_\mathrm{Comm} = (max_{i}^{i\in[1,N]} T_i - T_{i}^{M})/T

Please note that, when :math:`M = 1` (executions with one OpenMP thread or
without OpenMP all together), :math:`\mathrm{Hyb}_\mathrm{Eff} = \mathrm{MPI}_\mathrm{Eff}`.


Hybrid parallel efficiency
--------------------------

Let :math:`\mathrm{Hyb}_\mathrm{Eff}` (*Hybrid Parallel Efficiency*) be the efficiency considering both time in MPI calls and time in idle OpenMP threads as lost:

.. math::
    \mathrm{Hyb}_\mathrm{Eff} = T^U / T



Interaction between MPI and OpenMP metrics
------------------------------------------

There are certain situations in which threads are idling waiting for an MPI communication to finish.

On the OpenMP side, communication and computation can be overlapped to mitigate this inefficiency.

On the MPI side, reducing the time of the communication would also reduce the time that OpenMP threads are waiting.

Despite both programming models might be at fault, the current formulation of the hybrid model classifies this situation under the OpenMP metrics.

There is an ongoing work to incorporate interactions between MPI and OpenMP to the hybrid efficiencies model.
