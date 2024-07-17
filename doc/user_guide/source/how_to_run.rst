
.. highlight:: bash

*******************
How to run with DLB
*******************

Generally, in order to run DLB with your application, you need to follow these steps:

1. Link or preload (via ``LD_PRELOAD``) your application with any flavour of
   the DLB shared libraries. We recommend ``libdlb.so`` for simplicity, although you can
   choose other flavours like MPI, debug or instrumentation. Note, that you can also choose a different version of the library at runtime via the ``LD_PRELOAD`` mechanism. 

2. Configure the environment variable ``DLB_ARGS`` with the desired DLB options.
   Typically, you will want to set at least ``--lewi``, ``--drom``, ``-talp```,
   or any combination of them. Execute ``dlb --help`` for a list of options

3. Run you application as you would normally do.

For more information we provide more detailed information depending on the component used:

* :ref:`DROM <drom>`
* :ref:`LeWI <lewi>`
* :ref:`TALP <talp>`

