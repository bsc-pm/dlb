
.. highlight:: bash

*******************
How to run with DLB
*******************

In general, the following steps are required to run DLB with your application:

1. Link or preload (via ``LD_PRELOAD``) your application with any flavour of
   the DLB shared libraries. When linking, we recommend ``libdlb.so`` for
   simplicity. Then, at runtime, set the ``LD_PRELOAD`` environment variable
   to any other flavour such as MPI, debug or instrumentation.

2. Configure the environment variable ``DLB_ARGS`` with the desired DLB options.
   Typically, you will want to set at least ``--lewi``, ``--drom``, ``-talp```,
   or any combination of them. Execute ``dlb --help`` for a list of options.
   You can also use the :ref:`scripts` provided in the installation.

3. Run you application as you would normally do.

If you want to use the :ref:`API <api>` you might need to link with DLB.

.. _cmake-info:

Starting with version 3.5, we provide a CMake Config file to make it easier to
use DLB in CMake based projects:

.. code-block:: cmake

   find_package(DLB 3.5 REQUIRED) # Note that the version is optional
   target_link_libraries(<your_target> DLB::DLB)


For more information we provide more detailed information depending on the component used:

* :ref:`DROM <drom>`
* :ref:`LeWI <lewi>`
* :ref:`TALP <talp>`


.. _examples:

Examples
========

1. MPI application that does not require the usage of the DLB API::

    export DLB_ARGS="..."
    mpirun ... env LD_PRELOAD=<dlb_install_path>/lib/libdlb_mpi.so <app> <args>

2. Same as (1), but using the provided scripts (using ``talp.sh`` as an
   example)::

    cp <dlb_install_path>/share/doc/dlb/scripts/talp.sh .
    edit talp.sh     # Optional, review and edit DLB_ARGS
    mpirun ... ./talp.sh <app> <args>

3. MPI application that requires the usage of the DLB API (linking required)::

    # Compile and link you application with the base DLB library in order to
    # just satisfy the requirement of DLB symbols and avoid other dependences
    DLB_CPPFLAGS="-I<dlb_install_path>/include"
    DLB_LDLAGS="-L<dlb_install_path>/lib -ldlb -Wl,-rpath,<dlb_install_path>/lib"
    mpicc ... -o <app> $DLB_CPPFLAGS $DLB_LDFLAGS

    # Run as the examples above
    export DLB_ARGS="..."
    mpirun ... env LD_PRELOAD=<dlb_install_path>/lib/libdlb_mpi.so <app> <args>

4. Same as (3), but using the CMake script::

    export CMAKE_PREFIX_PATH+=":<dlb_install_path>/lib"
    cmake <source-dir> ...

    # Run as the examples above
    export DLB_ARGS="..."
    mpirun ... env LD_PRELOAD=<dlb_install_path>/lib/libdlb_mpi.so <app> <args>
