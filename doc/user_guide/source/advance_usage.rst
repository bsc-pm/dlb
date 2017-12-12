**************
Advanced Usage
**************

.. highlight:: cpp

.. note::
    Section in progress

.. _callbacks:


=================
DLB Return Errors
=================

::

    DLB_NOUPDT
    DLB_NOTED
    DLB_SUCCESS
    DLB_ERR_UNKNOWN
    DLB_ERR_NOINIT
    DLB_ERR_INIT
    DLB_ERR_DISBLD
    DLB_ERR_NOSHMEM
    DLB_ERR_NOPROC
    DLB_ERR_PDIRTY
    DLB_ERR_PERM
    DLB_ERR_TIMEOUT
    DLB_ERR_NOCBK
    DLB_ERR_NOENT
    DLB_ERR_NOCOMP
    DLB_ERR_REQST
    DLB_ERR_NOMEM
    DLB_ERR_NOPOL

=========
Callbacks
=========

Applications and runtimes need to register some callbacks just after initializing DLB. Here's
an example::

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


.. _asynchronous:

=================
Asynchronous mode
=================

DLB works on polling mode but the asynchronous mode can be enabled using the option
``DLB_ARGS+=" --mode=async"``.

In this mode, DLB creates a helper thread that will manage all the callbacks whenever the
status of a CPU changes.

Requests
========

In asynchronous mode, DLB can manage CPU requests through a petition queue. If a process
demands more resources than what DLB can provide, the petition is annotated in a queue
and will be satisfied as soon as some CPU becomes available.

This system is inherent to the asynchronous mode and the developer doesn't need to change
anything, but a few points to consider:

* Don't use any private data inside the callback functions, since these are called from
  and external thread, managed by DLB, that may not have access.
* Don't ignore return errors from DLB. The petition queue is finite and DLB can return
  ``DLB_ERR_REQST`` if the system cannot accept more petitions from a certain CPU.
* If some CPU request was made through ``DLB_AcquireCpu(int cpuid)``, this request can be
  revoked calling ``DLB_LendCpu(int cpuid)`` or ``DLB_Lend()``. [#f1]_
* If some CPU request was made through ``DLB_AcquireCpus(int ncpus)``, this request can be
  revoked calling ``DLB_AcquireCpus(0)``. [#f1]_

.. [#f1] This logic may change in the future. Currently there are two types of queues
        (specific CPUs, and unspecific) and we could consider to clear both queues using
        the same function, Lend all or Acquire(0) could do the same.

