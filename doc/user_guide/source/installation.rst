
.. highlight:: bash

*******************
How to install DLB
*******************

Build requirements
==================

* A supported platform running GNU/Linux (i386, x86-64, ARM, PowerPC or IA64)
* C compiler
* Python 2.4 or higher (Python 3 recommended)


Installation steps
==================

#. Get the latest DLB *tarball* from https://pm.bsc.es/dlb-downloads::

    tar xzf dlb-x.y.tar.gz
    cd dlb-x.y/

#. Configure it, with optionally some of the :ref:`DLB configure flags<dlb-configure-flags>`::

    ./configure --prefix=<dlb_install_path> [--with-mpi]

#. Build and install::

    make
    make install

Other installation methods
==========================

Downloading from git repository
-------------------------------

If DLB is downloaded from https://github.com/bsc-pm/dlb or other git
repository, additional software is needed, such as autoconf, automake, and
libtool. Once the project is downloaded, run ``bootstrap.sh`` to generate the
appropriate build files. Then, follow the installation steps described above
in steps 2 and 3.

Meson
-----

DLB also offers the possibility to configure and build with Meson and Ninja.
The current meson build script does not provide all the functionality of the
*autotools* scripts, in particular documentation and examples are not yet integrated,
but it is a great alternative for developers or quick installations as it
significantly improves build and test times.

To set up a meson build, run::

    meson setup <build_dir> -Dmpi=enabled -Dprefix=<dlb_install_path>
    cd <build_dir>
    ninja install

.. _dlb-configure-flags:

DLB configure flags
===================

Debug and Instrumentation versions
----------------------------------

By default, the *autotools scripts* will configure and build four versions of
the library, the combination of the performance and debug versions with the
instrumentation option. The basic library (performance, no-instrumentation)
cannot be disabled but the other three can be freely disabled using the
following flags.

--disable-debug
    Disable Debug library.
--disable-instrumentation
    Disable Instrumentation library.
--disable-instrumentation-debug
    Disable Instrumentation-Debug library.

Optional dependencies
---------------------

MPI allows DLB to automatically detect some patterns about the load balance of
the application. When MPI support is detected, another set of libraries with
prefix ``libdlb_mpi*`` are built; refer to :ref:`mpi-interception` for more
details.

--with-mpi=<mpi_prefix>
    Specify where to find the MPI libraries and include files.

HWLOC allows DLB library to get some knowledge about the hardware details
of the compute node. If HWLOC is not found, hardware detection will fall back
to some OS utilities.

--with-hwloc=<hwloc_prefix>
    Specify where to find the HWLOC libraries.

PAPI allows DLB to collect hardware counters. If PAPI is found TALP will also
measure the IPC of the instrumented regions.

--with-papi=<papi_prefix>
    Specify where to find the PAPI libraries.

.. _dlb-mpi-confgure-flags:

Additional MPI configure flags
------------------------------

The DLB MPI library ``libdlb_mpi.so`` defines, and thus may intercept, both C
and Fortran MPI symbols. In some systems, having both symbols together may
suppose a problem. If needed, DLB can be configured to generate additional
libraries with only either C or Fortran MPI symbols.

--enable-c-mpi-library
    Compile also a DLB MPI library specific for C
--enable-fortran-mpi-library
    Compile also a DLB MPI library specific for Fortran

DLB will compile by default the Fortran 2008 MPI interface if a suitable
Fortran compiler is found. This interface is needed to intercept such
F08 MPI calls. However, if the compilation failed for some reason, the
interface compilation may be disabled.

--disable-f08-mpi-interface
    Disable Fortran 2008 MPI interface
