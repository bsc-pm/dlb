**************
Advanced Usage
**************

.. highlight:: cpp

=================
DLB Return Errors
=================

Most of the functions of the DLB API return an error code to indicate whether the
operation was successful or DLB found any kind of error. These error codes can be
passed to the function ``const char* DLB_Strerror(int errnum)`` to obtain a human
readable description. The possible error codes are::

    // Positive error codes
    DLB_NOUPDT          "The requested operation does not need any action"
    DLB_NOTED           "The operation cannot be performed now, but it has been attended"

    // Zero error code
    DLB_SUCCESS         "The operation was successful"

    // Negative error codes
    DLB_ERR_UNKNOWN     "Unknown error"
    DLB_ERR_NOINIT      "DLB has not been initialized"
    DLB_ERR_INIT        "DLB is already initialized"
    DLB_ERR_DISBLD      "DLB is disabled"
    DLB_ERR_NOSHMEM     "DLB cannot find a shared memory"
    DLB_ERR_NOPROC      "DLB cannot find the requested process"
    DLB_ERR_PDIRTY      "DLB cannot update the target process, operation still in process"
    DLB_ERR_PERM        "DLB cannot acquire the requested resource"
    DLB_ERR_TIMEOUT     "The operation has timed out"
    DLB_ERR_NOCBK       "The callback is not defined and cannot be invoked"
    DLB_ERR_NOENT       "The queried entry does not exist"
    DLB_ERR_NOCOMP      "The operation is not compatible with the configured DLB options"
    DLB_ERR_REQST       "DLB cannot take more requests for a specific resource"
    DLB_ERR_NOMEM       "DLB cannot allocate more processes into the shared memory"
    DLB_ERR_NOPOL       "The operation is not defined in the current policy"


.. _callbacks:

=========
Callbacks
=========

DLB public functions usually operate on a set of CPUs. These operations often
yield different results for each resource. For instance, an acquire operation
on a CPU mask may be *successful* for some CPUs, *noted* for others, and
*forbidden* for the rest. The operations may also be resolved asynchronously so
DLB cannot immediately return an unique result for each one of them.
Therefore, the error code is simplified with the errors shown above.

In order to send the results for each resource and each type of operation,
either immediately or asynchronously, DLB needs some callback functions.
Usually, ``enable_cpu`` and ``disable_cpu`` are enough. Here's an example of
how to register these callbacks::

    /*** included in <dlb.h> **********************************************************/
    typedef enum dlb_callbacks_e {
        dlb_callback_set_num_threads,
        dlb_callback_set_active_mask,
        dlb_callback_set_process_mask,
        dlb_callback_add_active_mask,
        dlb_callback_add_process_mask,
        dlb_callback_enable_cpu,
        dlb_callback_disable_cpu
    } dlb_callbacks_t;

    int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg);
    int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg);
    /**********************************************************************************/

    void enable_cpu_callback(int cpuid, void *arg);
    void disable_cpu_callback(int cpuid, void *arg);

    int main(int argc, char *argv[])
    {
        DLB_Init(...);
        DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t) enable_cpu_callback, arg);
        DLB_CallbackSet(dlb_callback_disable_cpu, (dlb_callback_t) disable_cpu_callback, arg);
        ...
        DLB_Finalize();
    }

The following table contains the callbacks that DLB may call for each of the listed modes:

    +------------+--------------------------------+
    | Mode       | Available callbacks            |
    +============+================================+
    | LeWI       | dlb_callback_set_num_threads   |
    +------------+--------------------------------+
    | LeWI mask  | | dlb_callback_enable_cpu      |
    |            | | dlb_callback_enable_cpu_set  |
    |            | | dlb_callback_disable_cpu     |
    |            | | dlb_callback_disable_cpu_set |
    +------------+--------------------------------+
    | DROM       | dlb_callback_set_process_mask  |
    +------------+--------------------------------+


.. _asynchronous:

=================
Asynchronous mode
=================

DLB operations are synchronous by default. This means that each process needs
to poll to check if a certain resource can be either acquired or must be
returned.

Since DLB 2.0, the library can also start in asynchronous mode using the option
``DLB_ARGS+=" --mode=async"``. In this mode, DLB creates a helper thread that
will invoke the appropriate callbacks from each process whenever necessary.

Requests
========

In asynchronous mode, DLB implements a petition queue to manage CPU requests.
If a process demands more resources than what DLB can provide, the petition is
annotated in a queue and will be satisfied as soon as some CPU becomes
available.

This system is inherent to the asynchronous mode and the developer doesn't need to change
anything, but a few points to consider:

* Don't use any private data inside the callback functions, since these are called from
  and external thread, managed by DLB, that may not have access.
* Don't ignore return errors from DLB. The petition queue is finite and DLB can return
  ``DLB_ERR_REQST`` if the system cannot accept more petitions from a certain CPU.
* If some request is made to acquire a specific CPU through ``DLB_AcquireCpu(int cpuid)``,
  this request can be revoked by calling ``DLB_LendCpu(int cpuid)`` or ``DLB_Lend()``. [#f1]_
* If some request is made to acquire a number of unspecific CPUs through
  ``DLB_AcquireCpus(int ncpus)``, this request can be revoked by calling
  ``DLB_AcquireCpus(0)``. [#f1]_

.. [#f1] This logic may change in the future. Currently there are two types of
    queues (specific CPUs, and number of unspecific CPUs) and we could consider to
    clear both queues using the same function, Lend all or Acquire(0) could do the
    same.


.. _ompt:

====
OMPT
====

OMPT is the OpenMP Tool Interface defined in the OpenMP 5.0 standard. Using
this interface, any external library can be registered as an OpenMP Tool during
the process startup, and then register callbacks for a set of defined OpenMP
events.

By having OMPT support, the DLB library can now passively detect the parallel
regions of the application and automatically redistribute the CPUs among the
other processes. The main advantages when using DLB with OpenMP and OMPT
support are:

* **The application does not need to be modified with the DLB API:** DLB will
  lend and borrow CPUs between parallel regions and MPI calls depending on the
  value of the variable ``--lewi-ompt`` (explained just below).
* **DLB is able to manage the affinity of all the OpenMP threads:** DLB will
  now bind each OpenMP thread to a unique CPU, and new threads will be pinned
  to a new CPU when that CPU becomes available. Without OMPT support, DLB
  cannot manage the affinity of OpenMP threads.

Usage
=====

To enable OMPT support, DLB needs the option ``DLB_ARGS+=" --ompt"`` and the
OpenMP runtime linked to the application must support this feature. If you are
unsure of whether the OpenMP runtime you are using supports OMPT, you can run
the example located in ``$DLB_PREFIX/share/doc/dlb/examples/OMPT``.

We do recommend to explicitly set the environment variable
``OMP_WAIT_POLICY="passive"``, since even if *passive* may be the default
value, we have observed that a non null value may affect other implementation
specific variables of the OpenMP runtime, such as ``KMP_BLOCKTIME``. [#f2]_

Once OMPT is enabled on DLB, the user can also enable other DLB modules such as
DROM or LeWI with their respective flags. Furthermore, LeWI in OMPT can be
fine-tuned with the option ``--lewi-ompt`` with any combination of the values
``[mpi, borrow, lend]`` separated by ``:``. If *mpi* is set, LeWI will be
invoked before and after each eligible MPI call. If *borrow* is set, DLB will
try to borrow CPUs before each non nested parallel construct. If the flag
*lend* is set, DLB will lend all non used CPUs after each non nested parallel
construct.

The last necessary step to use DLB with OMPT support is to invoke the application
with the binary ``dlb_run``.

Summary
=======

* Set flag ``--ompt`` to enable OMPT support
* Set flag ``--drom`` if you want to enable the DROM module
* Set flags ``--lewi`` and ``--lewi-ompt=...`` if you want to enable LeWI
* Set ``OMP_WAIT_POLICY=passive``
* Run ``dlb_run ./application``

.. [#f2] https://reviews.llvm.org/D18577

