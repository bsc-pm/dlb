
.. highlight:: bash
.. _talp:

******************************************************
TALP for monitoring the Programming Model efficiencies
******************************************************

TALP is a low-overhead profiling tool for collecting performance metrics from applications
using MPI, OpenMP, OpenACC, CUDA, and HIP. Support for some of these programming models is
still experimental.

These metrics can be :ref:`reported at the end of the execution<talp_report_at_the_end>`
or queried at runtime through the :ref:`API <talp-api>`, which allows users to
define :ref:`user-defined regions<talp-custom-regions>` and gain detailed insight into
the performance of specific parts of the code.

An in-depth explanation of the metrics computed by TALP can be found :ref:`here <talp_metrics>`.

:ref:`Here <talp_options>` you can get an overview of the different runtime options available when using TALP.

A good way to get started is to just run your application and let TALP :ref:`report the metrics at the end of the execution<talp_report_at_the_end>`.

If you already have some executions of your application using TALP, you might want to check out :ref:`TALP-Pages <talp_pages>` which can generate some plots using your JSON files.

.. _talp_report_at_the_end:

Reporting POP metrics at the end of the execution
=================================================

After :ref:`installing DLB <dlb-installation>` you can use TALP depending on the used programming models to report metrics at the end of the execution.


.. note::
    Note that the flags shown below are the minimum requirements for TALP to report metrics at the end of the execution.
    You can also use ``--talp-output-file`` to generate CSV or JSON formatted files. 
    More info in the options section :ref:`below <talp_options>`.

.. _mpi-only_executions:

MPI-only executions
-------------------
To gather and report the MPI-based performance metrics, you can run your application with ``libdlb_mpi`` pre-loaded and activate TALP by setting the ``DLB_ARGS``:

.. code-block:: bash

    DLB_PREFIX="<path-to-DLB-installation>"

    export DLB_ARGS="--talp"
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

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


OpenMP-only executions
----------------------
As TALP relies on the OMPT interface to inspect runtime behavior of OpenMP, the runtime implementation needs to support this.
Below you can find a table of currently supported runtimes and the respective compilers.


.. _talp_supported_compilers:

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - Compiler / Runtime
     - Support for TALP OpenMP metrics
   * - LLVM-compilers like ``clang,clang++``
     - supported with version ``>=8.0.0``
   * - Intel Classic-compilers ``icc,icpc``
     - supported with version ``>=19.0.1``
   * - Intel LLVM-compilers ``icx,icpx``
     - supported
   * - GNU compilers ``gcc,g++,gfortran``
     - not supported (see note below)
   * - Cray clang compilers ``craycc,crayc++``
     - supported with version ``>=17.0.0``


If you are using any supported compiler in the table above to build your application, you can execute your application like this to gather OpenMP performance metrics with TALP:

.. code-block:: bash

    DLB_PREFIX="<path-to-DLB-installation>"

    export DLB_ARGS="--talp --ompt --talp-openmp" 
    # "--ompt" enables the OMPT tool in DLB
    # "--talp-openmp" enables collection of OpenMP metrics
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb.so" ./foo

.. note::
   Since LLVM's OpenMP runtime supports the GOMP interface, you can compile and
   link applications with GCC and explicitly link against the LLVM OpenMP
   runtime using:

     ``gcc your_app.c -L<llvm_openmp_lib_dir> -Wl,-rpath,<llvm_openmp_lib_dir> -lomp``

   This allows you to use the LLVM OpenMP runtime even when compiling with GCC.


.. note::
   The compilers listed here are relevant only for their default OpenMP runtimes.
   This does not refer to the compiler used to build the DLB library.


Hybrid (OpenMP+MPI) executions
------------------------------
For hybrid applications, TALP also needs an OpenMP runtime with OMPT support.
Please make sure that you use a :ref:`supported compiler <talp_supported_compilers>` to build your application.

The ``DLB_ARGS`` to configure TALP for hybrid applications are the same as for OpenMP ones, but this time ``libdlb_mpi.so`` is preloaded instead:

.. code-block:: bash

    DLB_PREFIX="<path-to-DLB-installation>"

    export DLB_ARGS="--talp --ompt --talp-openmp"
    # "--ompt" enables the OMPT tool in DLB
    # "--talp-openmp" enables collection of OpenMP metrics
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo


After your program has finished you will get a report similar to this on ``stderr`` at the end of the execution:

.. code-block:: bash

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

.. _talp_nvidia:

NVIDIA GPU executions
---------------------
For GPU applications running on NVIDIA devices, DLB must be previously be configured with
``--with-cuda`` (see :ref:`dlb-configure-flags`) so that it can locate the appropriate CUDA
and CUPTI libraries.

Usage is similar to the previous examples. The CUPTI plugin must be loaded explicitly
with ``--plugin=cupti``. Additionally, if the application is not an MPI program,
DLB may need to be auto-initialized with a special environment variable:

.. code-block:: bash

    # Pure GPU execution, DLB needs to be auto-intialized
    export DLB_AUTO_INIT=1
    export DLB_ARGS="--talp --plugin=cupti"
    LD_PRELOAD="$DLB_PREFIX/lib/libdlb.so" ./foo

.. code-block:: bash

    # Hybrid MPI+GPU execution, preload libdlb_mpi.so as usual, DLB_AUTO_INIT not needed
    export DLB_ARGS="--talp --plugin=cupti"
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

A hybrid MPI+GPU execution will result in an output similar to this::

    DLB[<hostname>:<pid>]: ############### Monitoring Region POP Metrics ################
    DLB[<hostname>:<pid>]: ### Name:                                     Global
    DLB[<hostname>:<pid>]: ### Elapsed Time:                             10.16 s
    DLB[<hostname>:<pid>]: ### Host
    DLB[<hostname>:<pid>]: ### ----
    DLB[<hostname>:<pid>]: ### Parallel efficiency:                      0.99
    DLB[<hostname>:<pid>]: ###  - MPI Parallel efficiency:               1.00
    DLB[<hostname>:<pid>]: ###     - Communication efficiency:           1.00
    DLB[<hostname>:<pid>]: ###     - Load Balance:                       1.00
    DLB[<hostname>:<pid>]: ###        - In:                              1.00
    DLB[<hostname>:<pid>]: ###        - Out:                             1.00
    DLB[<hostname>:<pid>]: ###  - Device Offload efficiency:             0.99
    DLB[<hostname>:<pid>]: ###
    DLB[<hostname>:<pid>]: ### NVIDIA Device
    DLB[<hostname>:<pid>]: ### -------------
    DLB[<hostname>:<pid>]: ### Parallel efficiency:                      0.50
    DLB[<hostname>:<pid>]: ###  - Load Balance:                          1.00
    DLB[<hostname>:<pid>]: ###  - Communication efficiency:              1.00
    DLB[<hostname>:<pid>]: ###  - Orchestration efficiency:              0.50


.. _talp_amd:

AMD GPU executions
------------------

For GPU applications running on AMD devices, DLB must be previously configured with
``--with-rocm`` (see :ref:`dlb-configure-flags`) so that it can locate the appropriate
ROCm libraries.

The usage is very similar to the NVIDIA case, but there are two possible
CUPTI-equivalent DLB plugins depending on the ROCm version:

- ``rocprofilerv2``: uses ROCm's deprecated ``librocprofilerv2``. While functional
  in some cases, it may fail to obtain metrics for reasons that are not fully understood.

- ``rocprofiler-sdk``: uses the officially supported ``librocprofiler-sdk``,
  available starting with ROCm 6.2 and the recommended method
  for collecting performance metrics on AMD GPUs.

In the examples below, we show the usage of the ``rocprofiler-sdk`` DLB plugin,
which is implemented a bit differently and doesn't need any automatic
initialization in case of non-MPI applications:

.. code-block:: bash

    # Pure GPU execution
    export DLB_ARGS="--talp --plugin=rocprofiler-sdk"
    LD_PRELOAD="$DLB_PREFIX/lib/libdlb.so" ./foo

.. code-block:: bash

    # Hybrid MPI+GPU execution
    export DLB_ARGS="--talp --plugin=rocprofiler-sdk"
    mpirun <options> env LD_PRELOAD="$DLB_PREFIX/lib/libdlb_mpi.so" ./foo

Hybrid MPI+GPU executions on AMD devices produce output comparable to the
NVIDIA example shown above. A separate example is omitted for brevity.

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
  using the flag ``--talp-region-select``, as explained explained :ref:`below<talp_options>`).

Basic usage examples for C:

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

Basic usage for Fortran:

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

And for Python:

.. code-block:: python

    import dlb
    ...
    monitor = dlb.DLB_MonitoringRegionRegister("Region 1")
    ...
    for ... :
        # Resume region
        dlb.DLB_MonitoringRegionStart(monitor)
        ...
        # Pause region
        dlb.DLB_MonitoringRegionStop(monitor)

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
    integer :: err
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


And for Python codes:

.. code-block:: python

    import dlb

    dlb_handle = dlb.DLB_MonitoringRegionRegister("Region 1")

    dlb.DLB_MonitoringRegionStart(dlb_handle)
    dlb.DLB_MonitoringRegionStop(dlb_handle)

    monitor = dlb_handle.contents

    print(monitor.name.decode())
    print(monitor.num_measurements)
    print(monitor.elapsed_time)


Special values for monitoring regions
-------------------------------------

The special values ``DLB_GLOBAL_REGION`` and ``DLB_LAST_OPEN_REGION`` can be
used in any TALP function to refer to these contexts without needing to pass
the region handle explicitly:

.. code-block:: c++

    // Helper class to create a region for the current scope
    struct Profiler {
        Profiler(const std::string& name) {
            dlb_monitor_t *monitor = DLB_MonitoringRegionRegister(name.c_str());
            DLB_MonitoringRegionStart(monitor);
        }

        ~Profiler() {
            DLB_MonitoringRegionStop(DLB_LAST_OPEN_REGION);
        }
    };

    void foo() {
        // Everything in this scope is recorded as "Region 1"
        {
            Profiler p("Region 1");
            ...
        }

        // Everything in this scope is recorded as "Region 2"
        {
            Profiler p("Region 2");
            ...
        }

        // Print current Global region metrics
        DLB_MonitoringRegionReport(DLB_GLOBAL_REGION);
    }

In Fortran, ``DLB_GLOBAL_REGION`` is defined as ``type(c_ptr)`` and can be
used similarly to how it's used in C. Additionally, ``DLB_GLOBAL_REGION_INT`` and
``DLB_LAST_OPEN_REGION_INT`` are defined as ``integer(kind=c_intptr_t)`` and must
be converted to ``type(c_ptr))`` using the F90 intrinsic procedure ``transfer``:

.. code-block:: fortran

    ! Print current Global region metrics
    err = DLB_MonitoringRegionReport(DLB_GLOBAL_REGION)

    ! Equivalent, using integer(c_intptr_t)
    err = DLB_MonitoringRegionReport(transfer(DLB_GLOBAL_REGION_INT, c_null_ptr))

    ! Start region and stop
    err = DLB_MonitoringRegionStart(dlb_handle)
    err = DLB_MonitoringRegionStop(transfer(DLB_LAST_OPEN_REGION_INT, c_null_ptr))

In Python, values ``DLB_GLOBAL_REGION`` and ``DLB_LAST_OPEN_REGION`` are used the same way as in C codes, without needing to pass the region handle explicitly:

.. code-block:: python

    import dlb

    # Print current Global region metrics
    dlb.DLB_MonitoringRegionReport(dlb.DLB_GLOBAL_REGION)

    # Start region and stop
    dlb.DLB_MonitoringRegionStart(dlb_handle)
    dlb.DLB_MonitoringRegionStop(dlb.DLB_LAST_OPEN_REGION)


Computing POP metrics for a region at run time
-----------------------------------------------

POP metrics can be obtained at any point by calling a collective DLB function
that gathers data from all processes. The call accepts a ``dlb_monitor_t`` to
specify a region, or ``DLB_GLOBAL_REGION`` for the implicit global region. The
returned struct contains both intermediate values and final efficiency ratios,
the latter shown below:

.. code-block:: c

    typedef struct dlb_pop_metrics_t {
        ...
        float parallel_efficiency;
        float mpi_parallel_efficiency;
        float mpi_communication_efficiency;
        float mpi_load_balance;
        float mpi_load_balance_in;
        float mpi_load_balance_out;
        float omp_parallel_efficiency;
        float omp_load_balance;
        float omp_scheduling_efficiency;
        float omp_serialization_efficiency;
    } dlb_pop_metrics_t;

Example in C:

.. code-block:: c

    #include <dlb_talp.h>
    ...
    dlb_monitor_t *monitor = DLB_MonitoringRegionRegister("Region 1");
    // The monitor is then used to start and stop the region.
    ...
    dlb_pop_metrics_t pop_metrics;
    // This call performs an MPI synchronization across all processes.
    int err = DLB_TALP_CollectPOPMetrics(monitor, &pop_metrics);
    printf("%1.2f\n", pop_metrics.parallel_efficiency);
    printf("%1.2f\n", pop_metrics.mpi_communication_efficiency);
    ...

Example in Fortran:

.. code-block:: fortran

    use iso_c_binding
    implicit none
    include 'dlbf_talp.h'
    type(c_ptr) :: dlb_handle
    type(dlb_pop_metrics_t) :: pop_metrics
    integer :: err
    ...
    dlb_handle = DLB_MonitoringRegionRegister(c_char_"Region 1"//C_NULL_CHAR)
    ! The dlb_handle is then used to start and stop the region.
    ...
    ! This call performs an MPI synchronization across all processes.
    err = DLB_TALP_CollectPOPMetrics(dlb_handle, pop_metrics)
    print *, pop_metrics%parallel_efficiency
    print *, pop_metrics%mpi_communication_efficiency

For Python MPI programs, the ``dlb_mpi`` module must be used instead of the base ``dlb`` module. These calls also require running with the :ref:`DLB MPI library preloaded <mpi-only_executions>`.


.. code-block:: python

    import dlb_mpi
    ...
    dlb_handle = dlb_mpi.DLB_MonitoringRegionRegister("Region 1")
    # The dlb_handle is then used to start and stop the region.
    ...
    # This call performs an MPI synchronization across all processes.
    pop_metrics = dlb_mpi.DLB_TALP_CollectPOPMetrics(dlb_handle)

    print(f"Parallel efficiency: {pop_metrics.parallel_efficiency:.2f}")
    print(f"MPI communication efficiency: {pop_metrics.mpi_communication_efficiency:.2f}")


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

    The accepted formats are JSON and CSV, selected based on the file extensions
    ``*.json`` and ``*.csv``, respectively. JSON files will be overwritten if they
    already exist, while CSV files will be appended as new rows. Any other file
    extension will result in plain text output.

    **Deprecated formats:**

    The ``*.xml`` file extension is deprecated and will be removed in the next release.


--talp-region-select=<string>
    Select TALP regions to enable. This option follows the format:
    ``--talp-region-select=[(include|exclude):]<region-list>``

    The modifiers ``include:`` and ``exclude:`` are optional, but only one
    modifier can be used at a time. If neither is specified, ``include:`` is
    assumed by default.

    The ``<region-list>`` can be a comma-separated list of regions or a special
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

--plugin=<string>
    Select plugin to enable.

    For now, only ``cupti``, ``rocprofiler-sdk``, and ``rocprofilerv2``.
    (Experimental)
