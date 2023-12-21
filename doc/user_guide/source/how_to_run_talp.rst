
.. highlight:: bash

******************************************************
TALP for monitoring the Programming Model efficiencies
******************************************************

The TALP module is used to collect performance metrics of MPI applications (and
OpenMP in the near future), like MPI communication efficiency or load balance
coefficients.

TALP is a profiling tool with low overhead that allows to obtain performance
metrics at the end of the execution, as well as to query them at run-time, via
first-party code that executes in the address space of the program or a
third-party program.

The performance metrics reported by TALP are a subset of the `POP metrics
<https://pop-coe.eu/node/69>`_. The POP metrics provide a quantitative analysis
of the factors that are most relevant in parallelisation. The metrics are
calculated as efficiencies between 0 and 1 in a hierarchical representation,
with the top-level metric being the "Global Efficiency", which is decomposed in
"Computation Efficiency" and "Parallel Efficiency", and these two decomposed in
other finer-scope efficiencies and so on.

Some of the POP metrics need several runs with different numbers of resources
to compute them, such as the "Instruction Scaling" and "IPC Scaling". TALP only
measures performance metrics of the current application and thus, the POP
metrics that TALP reports are:

    * Parallel Efficiency
        * Communication Efficiency
        * Load Balance
            * Intra-node Load Balance (or LB_in)
            * Inter-node Load Balance (or LB_out)

The Parallel Efficiency corresponds to the global MPI efficiency; this is the
ratio of useful computation versus the time lost due to MPI Communication
Serialization, MPI Communication Transfer and MPI Load Balance. TALP also
computes two new metrics not defined in the POP Standard Metrics; Load Balance
can actually be decomposed in two more metrics, the Intra-node Load Balance
that corresponds to the Load Balance inside the node of the most loaded node
(the limiting factor), and the Inter-node Load Balance that identifies the Load
Balance among nodes.


Reporting POP metrics at the end of the execution
=================================================

Simply run::

    $ export DLB_ARGS="--talp"
    $ mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

Or::

    $ mpirun <options> "$DLB_PREFIX/share/doc/dlb/scripts/talp.sh" ./foo

And you will obtain a report similar to this one at the end::

    DLB[<hostname>:<pid>]: ######### Monitoring Region App Summary #########
    DLB[<hostname>:<pid>]: ### Name:                       MPI Execution
    DLB[<hostname>:<pid>]: ### Elapsed Time :              12.87 s
    DLB[<hostname>:<pid>]: ### Parallel efficiency :       0.70
    DLB[<hostname>:<pid>]: ###   - Communication eff. :    0.91
    DLB[<hostname>:<pid>]: ###   - Load Balance :          0.77
    DLB[<hostname>:<pid>]: ###       - LB_in :             0.79
    DLB[<hostname>:<pid>]: ###       - LB_out:             0.98


POP Metrics Overview
=================================================
As already pointed out above, TALP is able to generate some of the POP Metrics per region.
In this section we provide some insight into how they compute and what they tell.
For all the calculations below, we assume that :math:`T_^{u}_{i}` is the time the :math:`i`-th MPI process spends in application code doing useful work. 
Note, that this explicitly excludes any time spent in MPI.
Furthermore we also define :math:`T^{e}_{i}` to be the total runtime of the :math:`i`-th MPI process including MPI.
Also we denote :math:`N_{p}` as the number of processes available in ``MPI_COMM_WORLD``
Let :math:`\mathbb{N}_{j}` denote the index set containing the MPI process indeces :math:`i` beeing located at Node :math:`j`. 

----------
 Parallel Efficiency
----------
.. math::
    \frac{ \sum_{i}^{N_{process}} T^{useful}_{i} }{\max_{i} T^{elapsed}_{i} \times N_{process} }



----------
 Communication Efficiency
----------
.. math::
    \frac{ \max_{i} T^{useful}_{i} }{ \max_{i} T^{elapsed}_{i} }
----------
 Load Balance
----------
.. math::
    \frac{ \sum_{i}^{N_{process}} T^{useful}_{i} }{ \max_{i} T^{useful}_{i} \times N_{process}}

----------
 Intra-node Load Balance (LB_in)
----------
.. math::
    \frac{ \max_{j} (\sum_{i \in \mathbb{N}_{j}}^{N_{process}} T^{useful}_{i}) }{ \max_{i} T^{useful}_{i} \times N_{process} }

----------
 Inter-node Load Balance
----------
.. math::
    \frac{  \sum_{i}^{N_{process}} T^{useful}_{i} }{ (\sum_{i \in \mathbb{N}_{j}}^{N_{process}} T^{useful}_{i}) \times N_{process}}


----------
 Average IPC
----------

Parallel Efficiency
Communication Efficiency
Load Balance
Intra-node Load Balance (or LB_in)
Inter-node Load Balance (or LB_out)
Average IPC (only available with PAPI enabled)


Definining custom monitoring regions
====================================

By default, TALP reports the entire MPI execution from ``MPI_Init`` to
``MPI_Finalize``. Applications may also use user-defined regions to monitor
different sub-regions of the code.

A monitoring region may be registered using the funtion
``DLB_MonitoringRegionRegister``; multiple calls with the same non-null
char pointer will return the same region. A region will not start until
the function ``DLB_MonitoringRegionStart`` is called, and needs to
finish with the function ``DLB_MonitoringRegionStop`` at some point
before ``MPI_Finalize``. A monitoring region may be paused and resumed
multiple times. Basic usage example for C and Fortran:

.. code-block:: c

    #include <dlb_talp.h>
    ...
    dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Region 1");
    ...
    while (...) {
        ...
        /* Resume region */
        DLB_MonitoringRegionStart(monitor);
        ...
        /* Pause region */
        DLB_MonitoringRegionStop(monitor);
    }

.. code-block:: fortran

    use iso_c_binding
    implicit none
    include 'dlbf_talp.h'
    type(c_ptr) :: dlb_handle
    integer :: err
    ...
    dlb_handle = dlb_monitoringregionregister(c_char_"Region 1"//C_NULL_CHAR)
    ...
    do ...
        ! Resume region
        err = DLB_MonitoringRegionStart(dlb_handle)
        ...
        ! Pause region
        err = DLB_MonitoringRegionStop(dlb_handle)
    enddo

For every defined monitoring region, including the implicit global region
name "MPI Execution", TALP will print or write a summary at the end of the
execution.

Inspecting monitoring regions within the source code
----------------------------------------------------

The struct ``dlb_monitor_t`` is defined in ``dlb_talp.h``. Its fields may be
accessed at any time, although some of them may lack some consistency if the
region is not stopped.

For Fortran codes, the struct may be accessed like in this example:

.. code-block:: fortran

    use iso_c_binding
    implicit none
    include 'dlbf_talp.h'
    type(c_ptr) :: dlb_handle
    type(dlb_monitor_t), pointer :: dlb_monitor
    integer :: ierr
    character(16), pointer :: monitor_name
    ...
    dlb_handle = dlb_monitoringregionregister(c_char_"Region 1"//C_NULL_CHAR)
    err = DLB_MonitoringRegionStart(dlb_handle)
    err = DLB_MonitoringRegionStop(dlb_handle)
    ...
    call c_f_pointer(dlb_handle, dlb_monitor)
    call c_f_pointer(dlb_monitor%name_, monitor_name)
    print *, monitor_name
    print *, dlb_monitor%num_measurements
    print *, dlb_monitor%elapsed_time


Redirecting TALP output to a file
=================================

Use the flag ``--talp-output-file=<path>`` to select the output file of the
TALP report, instead of the default printing to ``stderr``.

TALP accept several output formats, which will be detected by file extension:
``*.json``, ``*.xml``, and ``*.csv``. If the output file extension does not
correspond to any of them, the output will be in plain text format. Note that
for JSON and XML formats, TALP will overwrite the existing file, while if the
file is CSV, TALP will append all executions as new rows. This may change in
the future in favor of appending also to JSON and XML formats.

Enabling Hardware Counters
==========================

:ref:`Configure<dlb-configure-flags>` DLB with ``--with-papi`` and add
``--talp-papi`` to ``DLB_ARGS``. With PAPI enabled, TALP will also report the
average IPC.


TALP option flags
=================

--talp-papi=<bool>
    Select whether to collect PAPI counters.

--talp-summary=<none:all:pop-metrics:pop-raw:node:process>
    List of summaries, separated by ``:``, to write at the end: ``pop-metrics``
    is the default option, ``pop-raw`` shows the  raw metrics used to compute
    the POP metrics, ``node`` and ``process`` show a summary for each node and
    process respectively.

--talp-external-profiler=<bool>
    Enable live metrics update to the shared memory. This flag is only needed
    if there is an external program monitoring the application.

--talp-output-file=<path>
    Write TALP metrics to a file. If this option is not provided, the output is
    printed to stderr. Accepted formats: \*.json, \*.xml, \*.csv. Any other for
    plain text.

--talp-regions-per-proc=<int>
    Number of TALP regions per process to allocate in the shared memory.

