
.. highlight:: bash

.. _dlb-installation:

*******************
How to install DLB
*******************

Build requirements
==================

* A supported platform running GNU/Linux (i386, x86-64, ARM, PowerPC, IA64, or RISC-V)
* C compiler
* Python 2.7 or higher (Python 3 recommended; required for some optional components)


Installation steps
==================

#. Get the latest DLB *tarball* from https://pm.bsc.es/dlb-downloads::

    tar xzf dlb-x.y.z.tar.gz
    cd dlb-x.y.z/

#. Configure it, with optionally some of the :ref:`DLB configure flags<dlb-configure-flags>`::

    ./configure --prefix=<dlb_install_path> [--with-mpi]

#. Build and install::

    make [-j N]
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

By default, the *autotools scripts* will configure and build four variants of
the library, the combination of the performance and debug variants with the
instrumentation variant. The main library ``libdlb.so`` (performance,
no-instrumentation) cannot be disabled but the other three can be freely
disabled using the following flags.

--disable-debug
    Disable Debug variant library; otherwise installed as ``libdlb_dbg.so``.
--disable-instrumentation
    Disable Instrumentation variant library; otherwise installed as ``libdlb_instr.so``.
--disable-instrumentation-debug
    Disable Instrumentation-Debug variant library; otherwise installed as ``libdlb_instr_dbg.so``.

Optional dependencies
---------------------

MPI allows :ref:`LeWI<lewi>` to automatically detect some patterns about the load balance of
the application, and :ref:`TALP<talp>` to gather MPI metrics. When MPI support is detected,
another set of libraries with prefix ``libdlb_mpi*`` are built; refer to
:ref:`mpi-interception` for more details.

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

CUPTI libraries from CUDA allow TALP to obtain performance metrics from NVIDIA GPUs.

--with-cuda=<cuda_prefix>
    Specify where to find CUDA and CUPTI libraries.

ROCm may provide support for rocprofilerv2 (now deprecated and only half
functional with DLB) and for the rocprofiler-sdk (available starting with ROCm
6.2; recommended). These libraries enable TALP to collect performance metrics from AMD GPUs.

--with-rocm=<rocm_prefix>
    Specify where to find ROCm profiling libraries.

.. _dlb-mpi-confgure-flags:

Additional MPI configure flags
------------------------------

The DLB MPI library ``libdlb_mpi.so`` defines, and thus may intercept, both C
and Fortran MPI symbols. In some systems, having both symbols together may
suppose a problem. If needed, DLB can be configured to generate additional
libraries with only either C or Fortran MPI symbols.

--enable-c-mpi-library
    Compile also a DLB MPI library specific for C, installed as ``libdlb_mpic.so``.
--enable-fortran-mpi-library
    Compile also a DLB MPI library specific for Fortran, installed as ``libdlb_mpif.so``.

DLB will compile by default the MPI Fortran 2008 MPI bindings if a suitable
Fortran compiler is found and the MPI library supports them. These bindings
are needed to intercept such MPI F08 calls. However, if the compilation failed
for some reason, the bindings may be explicitly disabled.

--disable-f08-mpi-bindings
    Disable MPI Fortran 2008 bindings
--disable-mpi-f08ts-bindings
    Disable MPI Fortran 2008 + TS 29113 bindings
