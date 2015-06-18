*******************
Public API
*******************

The DLB API can be divided in three parts:

**MPI API**
  This is a specific API for MPI. We offer an MPI interface that will be called by Extrae if we are tracing the application or internally in the MPI intercept API. All the calls of this API are of the form:

  - DLB_<mpi_call_name>_enter(...)

  - DLB_<mpi_call_name>_leave(...)

 
**Basic set**
  The basic set is very simple and reduced and oriented to application developers. The different functions will be explained in detail in section~\ref{sec:basic_api}.

**Advanced set**
  The advanced set is oriented to programming model runtimes but can be used by applications also. The advanced functions will be explained in detail in section~\ref{sec:advanced_api}.

====================
Basic set of DLB API
====================

This API is intended to give hints from the application to DLB. With this hints DLB is able to make a more efficient use of resources.

``void DLB_disable(void)``
  Will disable any DLB action. And reset the resources of the process to its default. While DLB is disabled there will not be any movement of threads for this process. Useful to limit parts of the code were DLB will not be helpful, by disabling DLB we avoid introducing any overhead.

``void DLB_enable(void)``
  Will enable DLB. DLB is enabled by default when the execution starts. And if it was not previously disable it will not have any effect. Useful to finish parts of the code were we disabled DLB temporally.

``void DLB_single(void)``
  Will lend all the threads of the process except one. Useful to mark parts of the code that are serial. The remaining threads can be used by some other process. All the DLB functions will be disabled except lending the thread when entering an MPI call, exiting and MPI call, DLB_parallel and DLB_enable.

``void DLB_parallel(void)``
  Will claim the default threads and enable all the DLB functions. Useful when exiting a serial section of code.

.. image:: images/dlb_states.png
  :width: 300pt
  :align: center
  :alt: DLB states diagram
 
We can summarize the behavior of these functions with the states graph shown in figure~\ref{fig:dlb_states}. We can consider three states for DLB (for each process) *Enabled*, *Disabled* and *Single*. *Enabled* would be the default state, where DLB will react to any API call. 

The *Disabled* state will not allow any change in the number of threads (only a call to ``DLB_enable`` will have effect). The number of threads of the process in *Disabled* state will be the default. 

The *Single* state will only react at ``DLB_enable`` or ``DLB_parallel`` API calls. The number of threads of the process in the *Single* state will be 1.

=======================
Advanced set of DLB API
=======================

The advanced set of calls is designed to be used by runtimes, either in the outer level or the inner level of parallelism. But advanced users can also use them from applications.


``void DLB_Init(void)``
  Initialize the DLB library and all its internal data structures. Must be called once and only one by each process in the DLB system.
 
``void DLB_Finalize(void)``
  Finalize the DLB library and clean up all its data structures. Must be called by each process before exiting the system.
 
``void DLB_reset(void)``
  Reset the number of threads of this process to its default.
 
``void DLB_UpdateResources(void)``
  Check the state of the system to update your resources. You can obtain more resources in case there are available cpus.
 
``void DLB_UpdateResources_max(int max_resources)``
  Check the state of the system to update your resources. You can obtain more resources in case there are available cpus. The maximum number of resources that you can get is ``max_resources``.
 
``void DLB_ReturnClaimedCpus(void)``
  Check if any of the resources you are using have been claimed by its owner and return it if necessary.
 
``void DLB_Lend(void)``
  Lend all your resources to the system. Except in case you are using the *1CPU* block mode you will lend all the resources except one cpu.
 
``void DLB_Retrieve(void)``
  Retrieve all your default resources previously lent.
 
``int DLB_ReleaseCpu(int cpu)``
  Lend this cpu to the system. The return value is 1 if the operation was successful and 0 otherwise.
 
``int DLB_ReturnClaimedCpu(int cpu)``
  Return this cpu to the system in case it was claimed by its owner. The return value is 1 if the cpu was returned to its owner and 0 otherwise. 
 
``void DLB_ClaimCpus(int cpus)``
  Claim as many cpus as the parameter ``cpus`` indicates. You can only claim your cpus. Therefor if you are claiming more cpus than the ones that you have lent, you will only obtain as many cpus as you have lent.
 
``void DLB_AcquireCpu(int cpu)``
  Notify the system that you are going to use this cpu. The system will try to adjust himself to this requirement, This function may leave the system in an unstable state. Avoid using it.
 
``int DLB_CheckCpuAvailability(int cpu)``
  This function returns 1 if your cpu is available to be used, 0 otherwise. Only available for policies with autonomous threads.
 
``int DLB_Is_auto(void)``
  Return 1 if the policy allows autonomous threads 0 otherwise.