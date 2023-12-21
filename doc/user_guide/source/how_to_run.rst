
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

Jump into the following sections for a more detailed description of each use case:

.. toctree::
    :maxdepth: 1

    how_to_run_lewi
    how_to_run_drom
    how_to_run_talp
