*******************
How to install DLB
*******************

Build requirements
==================

* A supported platform running Linux (i386, x86-64, ARM, PowerPC or IA64).
* GNU C/C++ compiler versions 4.4 or newer.
* Python 2.4 or newer.


Installation steps
==================

#. Get the latest DLB *tarball* from https://pm.bsc.es/dlb-downloads. Unpack the
   file and enter the new directory::

    $ tar xzf dlb-x.y.tar.gz
    $ cd dlb-x.y/

#. Configure it, with optionally some of the :ref:`DLB configure flags<dlb-configure-flags>`::

   $ ./configure --prefix=<<DLB_PREFIX>>

#. Build and install::

   $ make
   $ make install

.. note::
    If you plan to compile `Nanos++`_ and `Mercurium`_ later, consider adding
    ``--with-dlb=<<DLB_PREFIX>>`` to their configure options.

.. _Nanos++: https://pm.bsc.es/nanox
.. _Mercurium: https://pm.bsc.es/mcxx

.. _dlb-configure-flags:

DLB configure flags
===================

By default, the *autotools scripts* will build four versions of the library, the combination of
the performance and debug versions with the instrumentation option. The basic library
(performance, no-instrumentation) cannot be disabled but the other three can be freely disabled
using the following flags.

--disable-debug
    Disable Debug library.
--disable-instrumentation
    Disable Instrumentation library.
--disable-instrumentation-debug
    Disable Instrumentation-Debug library.

DLB library has two optional dependencies. MPI allows DLB to automatically detect some patterns
about the load balance of the application. When MPI support is detected, another set of libraries
``libdlb_mpi*`` and ``libdlb_mpif*`` are built; refer to :ref:`mpi-interception` for more details.
The other optional dependency is HWLOC that allows DLB library to get some knowledge about the
hardware details of the compute node. If HWLOC is not found, hardware detection will fall back to
some OS utilities.

--with-mpi=<mpi_prefix>
    Specify where to find the MPI libraries and include files.
--with-hwloc=<hwloc_prefix>
    Specify where to find the HWLOC libraries.
