**********
Public API
**********

DLB offers a public interface for C, C++ and Fortran. The DLB API can be divided into:

.. glossary::

    Basic set
        The basic set contains the general purpose functions that are common to other
        modules. The different functions are explained in detail in section :ref:`basic-api`.

    LeWI: Lend When Idle
        The LeWI API is oriented to be used by runtimes to manage the CPU sharing between
        other processes but can also be used on applications to use the ``LeWI``
        algorithm. These functions are explained in detail in section
        :ref:`lewi-api`.

    DROM: Dynamic Resource Ownership Manager
        The DROM API manages the CPU ownership of each DLB running process. For a more
        detailed description see :ref:`drom_overview`. These functions are described in section
        :ref:`drom-api`.

    TALP: Tracking Application Live Performance
        The TALP API is used to obtain measured metrics from other processes as well as
        to define custom monitoring regions, see :ref:`talp_overview`. These functions are
        described in section :ref:`talp-api`.

    MPI API
        This is a specific API for MPI. We offer an MPI interface that will be called by
        Extrae if we are tracing the application or internally in the MPI intercept API.
        All the calls of this API are of the form shown below, and thus not documented.

        - DLB_<mpi_call_name>_enter(...)
        - DLB_<mpi_call_name>_leave(...)


..     TALP: Tracking Application Live Performance
..        To be done


=========
DLB Types
=========

The following types may be used in the DLB interface:

.. glossary::

    dlb_cpu_set_t
        Opaque type that corresponds to ``cpu_set_t *``. See ``<sched.h>``.

    const_dlb_cpu_set_t
        Opaque type that corresponds to ``const cpu_set_t *``.

    dlb_callbacks_t
        Enum to identify the type of callback. See ``"dlb_types.h"``.

    dlb_callback_t
        Opaque type for callback function.

    dlb_printshmem_flags_t
        Print shared memory flags. See ``"dlb_types.h"``.

    dlb_drom_flags_t
        DROM flags. See ``"dlb_types.h"``.

    dlb_monitor_t
        Monitoring region. See ``"dlb_talp.h"`` and :ref:`talp-custom-regions`.

    dlb_node_metrics_t
        Output struct where POP node metrics are stored. See ``"dlb_talp.h"``.

    dlb_pop_metrics_t
        Output struct where POP metrics are stored. See ``"dlb_talp.h"``.


.. _basic-api:

=============
DLB Basic API
=============

These functions make the basic API to be used independently from which DLB mode is enabled.

.. function:: int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args)

    Initialize DLB library and all its internal data structures. Must be called once and only
    once by each process in the DLB system.

.. function:: int DLB_Finalize(void)

    Finalize DLB library and clean up all its data structures. Must be called by each process
    before exiting the system.

.. function:: int DLB_Enable(void)

    Enable DLB and all its features in case it was previously disabled, otherwise it has no effect.
    It can be used in conjunction with ``DLB_Disable`` to delimit sections of the code where
    DLB calls will not have effect.

.. function:: int DLB_Disable(void)

    Disable DLB actions for the calling process. This call resets the original resources for the
    process and returns any external CPU it may be using at that time. While DLB is disabled there
    will not be any resource sharing for this process.

.. function:: int DLB_SetMaxParallelism(int max)

    Set the maximum number of resources to be used by the calling process. Useful to
    delimit sections of the code that the developer knows that only a maximum number of CPUs can
    benefit the execution. If a process reaches its maximum number of resources used at any
    time, subsequent calls to borrow CPUs will be ignored until some of them are returned.


.. function:: int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg)
              int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg)

    Setter and Getter for DLB callbacks. See section :ref:`callbacks`.

.. function:: int DLB_PollDROM(int *ncpus, dlb_cpu_set_t mask)
              int DLB_PollDROM_Update(void)

    Poll DROM module to check if the process needs to adapt to a new mask or number of CPUs.

.. function:: int DLB_SetVariable(const char *variable, const char *value)
              int DLB_GetVariable(const char *variable, char *value)

    Set or get a DLB internal variable. These variables are the same ones specified in ``DLB_ARGS``,
    although not all of them can be modified at runtime. If the variable is readonly the setter
    function will return an error.

.. function:: int DLB_PrintVariables(int print_extra)
              int DLB_PrintShmem(int num_columns, dlb_printshmem_flags_t print_flags)

    Print to stdout the information about the DLB internal variables and the status of the shared
    memories.

.. function:: const char* DLB_Strerror(int errnum)

    Obtain a string that describes the error code passed in the argument.

.. _lewi-api:

========
LeWI API
========

These functions are used to manage the CPU sharing between processes. Generally, each action may
have up to four different variants depending if the action is:

a) for all possible CPUs (no suffix)
b) for a specified CPU (Cpu suffix)
c) for a determined number of CPUs (Cpus suffix)
d) for a specified CPU mask (CpuMask suffix)

.. function:: int DLB_Lend(void)
              int DLB_LendCpu(int cpuid)
              int DLB_LendCpus(int ncpus)
              int DLB_LendCpuMask(const_dlb_cpu_set_t mask)

    Lend CPUs of the process to the system. A lent CPU may be assigned to other process that
    demands more resources. If the CPU was originally owned by the process it may be reclaimed.

.. function:: int DLB_Reclaim(void)
              int DLB_ReclaimCpu(int cpuid)
              int DLB_ReclaimCpus(int ncpus)
              int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask)

    Reclaim CPUs that were previously lent. It is mandatory that the CPUs belong to the
    calling process.

.. function:: int DLB_AcquireCpu(int cpuid)
              int DLB_AcquireCpus(int ncpus)
              int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask)

    Acquire CPUs from the system. If the CPU belongs to the process the call is equivalent
    to a *reclaim* action. Otherwise the process attempts to acquire a specific CPU in case
    it is available or enqueue a request if it's not.

.. function:: int DLB_Borrow(void)
              int DLB_BorrowCpu(int cpuid)
              int DLB_BorrowCpus(int ncpus)
              int DLB_BorrowCpuMask(const_dlb_cpu_set_t mask)

    Borrow CPUs from the system only if they are idle. No other action is done if the CPU
    is not available.

.. function:: int DLB_Return(void)
              int DLB_ReturnCpu(int cpuid)
              int DLB_ReturnCpuMask(const_dlb_cpu_set_t mask)

    Return CPUs to the system commonly triggered by a reclaim action from other process but
    stating that the current process still demands the usage of these CPUs. This action will
    enqueue a request for when the resources are available again.  If the caller does not want
    to keep the resource after receiving a *reclaim*, the correct action is *lend*.


.. _drom-api:

==================================
Dynamic Resource Manager Interface
==================================

The next set of functions can be used when the user has enabled the Dynamic Resource Ownership
Manager (DROM) Module (see :ref:`drom_overview`). With this interface the user can set or retrieve the
process mask of each DLB process.

.. function:: int DLB_DROM_Attach(void)

    Attach process to DLB as third party

.. function:: int DLB_DROM_Detach(void)

    Detach process from DLB

.. function:: int DLB_DROM_GetNumCpus(int *ncpus)

    Get the total number of available CPUs in the node

.. function:: void DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len)

    Get the PID's attached to this module

.. function:: int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask, dlb_drom_flags_t flags)

    Get the process mask of the given PID

.. function:: int DLB_DROM_SetProcessMask(int pid, const dlb_cpu_set_t mask, dlb_drom_flags_t flags)

    Set the process mask of the given PID


.. _talp-api:

==============
TALP Interface
==============

The TALP interface is divided in two sets of services. The first set provides the functionality
to obtain TALP data from an external process. This process needs first to attach to DLB
and later it can obtain some data from the other DLB running processes.

.. function:: int DLB_TALP_Attach(void)

    Attach process to DLB as third party

.. function:: int DLB_TALP_Detach(void)

    Detach process from DLB

.. function:: int DLB_TALP_GetNumCpus(int *ncpus)

    Get the total number of available CPUs in the node

.. function:: void DLB_TALP_GetPidList(int *pidlist, int *nelems, int max_len)

    Get the PID's attached to this module

.. function:: int DLB_TALP_GetTimes(int pid, double *mpi_time, double *useful_time)

    Get the CPU time on MPI and useful computation for the given process

.. function:: DLB_TALP_QueryPOPNodeMetrics(const char *name, dlb_node_metrics_t *node_metrics)

   Compute POP Node Metrics for one region


The second set of services are designed to be called from witihn the DLB running proceses.
With these funcions, the process can obtain live metrics from TALP, as well as to define
new custom Monitoring Regions to delimit a specific part of the code.

.. function:: const dlb_monitor_t* DLB_MonitoringRegionGetMPIRegion(void)

     Get the pointer of the implicit MPI Monitorig Region

.. function:: dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name)

    Register a new Monitoring Region

.. function:: int DLB_MonitoringRegionReset(dlb_monitor_t *handle)

    Reset monitoring region

.. function:: int DLB_MonitoringRegionStart(dlb_monitor_t *handle)

    Start (or resume) monitoring region

.. function:: int DLB_MonitoringRegionStop(dlb_monitor_t *handle)

    Stop (or pause) monitoring region

.. function:: int DLB_MonitoringRegionReport(const dlb_monitor_t *handle)

    Print a Report by stdout of the monitoring region

.. function:: int DLB_MonitoringRegionsUpdate(void)

    Explicitly update all monitoring regions

.. function:: int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics)

    Perform an MPI collective communication to collect POP metrics

.. function:: int DLB_TALP_CollectPOPNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics)

    Perform a node collective communication to collect TALP node metrics
