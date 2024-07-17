
.. highlight:: bash
.. _drom:

********************************************************************
DROM for making the application react to changes in the CPU affinity
********************************************************************

The DLB DROM module allows to modify the CPUs assigned to an existing process,
and not only the process affinity, also the thread affinity and the number of
active threads to correspond the new assigned mask.

A use case would be, for instance, a higher tier resource manager (e.g., Slurm)
modifying the resources of a job allocation. Even if the application is
malleable at OpenMP level, assigning or removing CPUs to the job allocation
will not make the application aware of the change. With DROM,
the application will detect it at some point and the number of threads
and their CPU affinity will be adjusted to reflect the changes of the
program new CPU affinity.


DROM example
============

DLB offers an API for third-party programs to attach to DLB and manage the
computational resources of other DLB running processes. DLB also offers the
``dlb_taskset`` binary, which is basically a wrapper for this API, to manually
configure the assigned CPUs of existing processes. It can also be used to
launch new processes. Run ``dlb_taskset --help`` for further info::

    # Binaries app_1, app_2 and app_3 are assumed to be linked with DLB
    $ export DLB_ARGS="--drom"
    $ ./app_1 &                         # app_1 mask: [0-7]
    $ dlb_taskset --remove 4-7          # app_1 mask: [0-3]
    $ taskset -c 4-7 ./app_2 &          # app_1 mask: [0-3], app_2 mask: [4-7]
    $ dlb_taskset -c 3,7 ./app_3 &      # app_1 mask: [0-2], app_2 mask: [4-6], app_3 mask: [3,7]
    $ dlb_taskset --list

Note that in the previous example, all programs will also adjust the number of
threads to match the number of CPUs in their CPU affinity mask as long as any of
these conditions applies:

    * The application's programming model is OmpSs and the runtime is
      properly configured with DLB.
    * The application's programming model is OpenMP and uses an OMPT-capable
      OpenMP runtime. Thus, for now only LLVM-based OpenMP runtimes. This case
      also requires setting ``DLB_ARGS+=" --ompt --ompt-thread-manager=omp5"``.
    * The application is linked with DLB and uses ``DLB_PollDROM`` to poll
      for changes in the CPU affinity mask.
