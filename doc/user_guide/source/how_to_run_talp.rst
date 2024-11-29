
.. highlight:: bash
.. _talp:

******************************************************
TALP for monitoring the Programming Model efficiencies
******************************************************

TALP is a low-overhead profiling tool used to collect performance metrics of MPI and OpenMP
applications (OpenMP metrics are still experimental).

These metrics can either be queried at runtime through an :ref:`API <talp-api>` or :ref:`reported at the end of the exection.<talp_report_at_the_end>`
You can get insight into the performance of different regions inside your code using :ref:`user-defined regions.<talp-custom-regions>`

An in-depth explanation of the metrics computed by TALP can be found :ref:`here <talp_metrics>`.

:ref:`Here <talp_options>` you can get an overview of the different runtime options available when using TALP.

A good way to get started is to just run your application and let TALP :ref:`report the metrics at the end of the execution.<talp_report_at_the_end>`

If you already have some executions of your application using TALP, you might wanna check out :ref:`TALP-Pages <talp_pages>` which can generate some plots using your JSON files.

.. _talp_report_at_the_end:

Reporting POP metrics at the end of the execution
=================================================

After :ref:`installing DLB <dlb-installation>` you can simply run::

    DLB_PREFIX="<path-to-DLB-installation>"

    export DLB_ARGS="--talp"
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

Or::

    mpirun <options> "$DLB_PREFIX/share/doc/dlb/scripts/talp.sh" ./foo

You will get a report similar to this on ``stderr`` at the end of the execution::

    DLB[<hostname>:<pid>]: ############### Monitoring Region POP Metrics ###############
    DLB[<hostname>:<pid>]: ### Name:                                     Global
    DLB[<hostname>:<pid>]: ### Elapsed Time:                             5 s
    DLB[<hostname>:<pid>]: ### Average IPC:                              0.23
    DLB[<hostname>:<pid>]: ### Parallel efficiency:                      0.40
    DLB[<hostname>:<pid>]: ### MPI Parallel efficiency:                  0.67
    DLB[<hostname>:<pid>]: ###   - MPI Communication efficiency:         0.83
    DLB[<hostname>:<pid>]: ###   - MPI Load Balance:                     0.80
    DLB[<hostname>:<pid>]: ###       - MPI Load Balance in:              0.80
    DLB[<hostname>:<pid>]: ###       - MPI Load Balance out:             1.00
    DLB[<hostname>:<pid>]: ### OpenMP Parallel efficiency:               0.60
    DLB[<hostname>:<pid>]: ###   - OpenMP Load Balance:                  0.80
    DLB[<hostname>:<pid>]: ###   - OpenMP Scheduling efficiency:         1.00
    DLB[<hostname>:<pid>]: ###   - OpenMP Serialization efficiency:      0.75


You can also use ``--talp-output-file`` to generate CSV or JSON formatted files. More info in the options section :ref:`below <talp_options>`.



.. _talp-custom-regions:

Defining custom monitoring regions
==================================

TALP utilizes *monitoring regions* to track and report performance metrics. A
monitoring region is a designated section of code marked for tracking.
Initially, TALP defines a default monitoring region, called "Global", which
spans from ``DLB_Init`` to ``DLB_Finalize``. Additionally, users can create
custom monitoring regions through the DLB API.

.. note::
   The region between ``DLB_Init`` and ``DLB_Finalize`` can vary depending
   on the initialisaton method used, whether it's automatic initialisation
   with MPI or OpenMP, or direct initialisation through the DLB API.

A monitoring region can be registered using the
``DLB_MonitoringRegionRegister`` function. Multiple calls with the same
non-null char pointer will return the same region. The region does not begin
until the function ``DLB_MonitoringRegionStart`` is called, and must end with
the function ``DLB_MonitoringRegionStop``.
A monitoring region may be paused and resumed multiple times.
All user-defined regions should be stopped before ``MPI_Finalize``.

Here are a few restrictions for naming monitoring regions:

- The name "Global" (case-insensitive) is reserved and cannot be used for any
  user-defined region. If the user attempts to register a region with this
  name, a pointer to the global region will be returned.
- The name "all" (case-insensitive) is reserved and cannot be
  used. Attempting to register a region with this name will result in an
  error.
- For user-defined regions, the name is case-sensitive, can contain up to
  128 characters, and may include spaces (though spaces must be avoided when
  using certain flags, as explained explained :ref:`below<talp_options>`).

Basic usage examples for C and Fortran:

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
    dlb_handle = DLB_MonitoringRegionRegister(c_char_"Region 1"//C_NULL_CHAR)
    ...
    do ...
        ! Resume region
        err = DLB_MonitoringRegionStart(dlb_handle)
        ...
        ! Pause region
        err = DLB_MonitoringRegionStop(dlb_handle)
    enddo

For each defined monitoring region, including the global region, TALP will
print or write a summary at the end of the execution.

.. note::
   See :ref:`Example 3 of How to run with DLB<examples>` for more information
   on compiling and linking with the DLB library.

Inspecting monitoring regions within the source code
----------------------------------------------------

The struct ``dlb_monitor_t`` is defined in ``dlb_talp.h``. Its fields can be
accessed at any time, although to guarantee that the values are up to date the
region needs to be stopped.

For Fortran codes, the struct can be accessed as in this example:

.. code-block:: fortran

    use iso_c_binding
    implicit none
    include 'dlbf_talp.h'
    type(c_ptr) :: dlb_handle
    type(dlb_monitor_t), pointer :: dlb_monitor
    integer :: ierr
    character(16), pointer :: monitor_name
    ...
    dlb_handle = DLB_MonitoringRegionRegister(c_char_"Region 1"//C_NULL_CHAR)
    err = DLB_MonitoringRegionStart(dlb_handle)
    err = DLB_MonitoringRegionStop(dlb_handle)
    ...
    call c_f_pointer(dlb_handle, dlb_monitor)
    call c_f_pointer(dlb_monitor%name_, monitor_name)
    print *, monitor_name
    print *, dlb_monitor%num_measurements
    print *, dlb_monitor%elapsed_time


Enabling Hardware Counters
==========================

:ref:`Configure<dlb-configure-flags>` DLB with ``--with-papi`` and add
``--talp-papi`` to ``DLB_ARGS``. With PAPI enabled, TALP will also report the
average IPC.

.. _talp_options:

TALP option flags
=================

--talp-openmp=<bool>
    Select whether to measure OpenMP metrics. (Experimental)

--talp-papi=<bool>
    Select whether to collect PAPI counters.

--talp-summary=<none:all:pop-metrics:process>
    Report TALP metrics at the end of the execution. If ``--talp-output-file`` is not
    specified, a short summary is printed. Otherwise, a more verbose file will be
    generated with all the metrics collected by TALP, depending on the list of
    requested summaries, separated by ``:``:

    ``pop-metrics``, the default option, will report a subset of the POP metrics.

    ``process`` will report the measurements of each process for each
    registered region.

    **Deprecated options:**

    ``pop-raw`` will be removed in the next release. The output will be
    available via the ``pop-metrics`` summary.

    ``node`` will be removed in the next release. Its data can be derived from
    the ``process`` report.

--talp-external-profiler=<bool>
    Enable live metrics update to the shared memory. This flag is only needed
    if there is an external program monitoring the application.

--talp-output-file=<path>
    Write extended TALP metrics to a file. If this option is omitted, the output is
    printed to stderr.
    The accepted formats are JSON and CSV, which are selected using the file
    extensions ``*.json`` and ``*.csv``, respectively. Any other file
    extension will result in plain text output.

    **Deprecated formats:**

    The ``*.xml`` file extension is deprecated and will be removed in the next release.


--talp-region-select=<string>
    Select TALP regions to enable. This option follows the format:
    ``--talp-region-select=[(include|exclude):]<region-list>``

    The modifiers ``include:`` and ``exclude:`` are optional, but only one
    modifier can be used at a time. If neither is specified, ``include:`` is
    assumed by default.

    The ``<region-list>`` can be a comma-separared list of regions or a special
    token ``all`` to refer to all regions. The global monitoring region may be
    specified with the special token ``global``. If the modifier ``include:``
    is used, only the listed regions will be enabled. If ``exclude:`` is used,
    all regions will be enabled except for the ones specified.

    Note that when using this feature, listed regions must not have spaces in
    their names.

    e.g.: ``--talp-region-select=all`` (default),
    ``--talp-region-select=exclude:all``,
    ``--talp-region-select=include:global,region3``,
    ``--talp-region-select=exclude:region4``.

--talp-regions-per-proc=<int>
    Number of TALP regions per process to allocate in the shared memory.

