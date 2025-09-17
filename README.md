# Dynamic Load Balancing Library

[![docs](https://readthedocs.org/projects/dlb-docs/badge/?version=latest)](https://dlb-docs.readthedocs.io/en/latest/)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15000657.svg)](https://doi.org/10.5281/zenodo.15000657)

[DLB][] is a dynamic library designed to speed up HPC hybrid applications (i.e.,
two levels of parallelism) by improving the load balance of the outer level of
parallelism (e.g., MPI) by dynamically redistributing the computational
resources at the inner level of parallelism (e.g., OpenMP). at run time.

This dynamism allows DLB to react to different sources of imbalance: Algorithm,
data, hardware architecture and resource availability among others.

#### Lend When Idle
LeWI (Lend When Idle) is the algorithm used to redistribute the computational
resources that are not being used from one process to another process inside
the same shared memory node in order to speed up its execution.

#### Dynamic Resource Ownership Manager
DROM (Dynamic Resource Ownership Manager) is the algorithm used to manage the
CPU affinity of a process running a shared memory programming model (e.g.,
OpenMP).

#### Tracking Application Live Performance
TALP (Tracking Application Live Performance) is the module used to gather
performance data from the application. The data can be obtained during
the execution or as a report at the end.


## Branching Strategy

Please note that the `main` branch is used as the development branch for this
project. It may contain experimental features and is not guaranteed to be
stable. For stable releases, please refer to the tagged versions in the
repository.


## Installation

1. Build requirements
    * A supported platform running GNU/Linux (i386, x86-64, ARM, PowerPC or IA64)
    * C compiler
    * Python 2.7 or higher (Python 3 recommended)
    * GNU Autotools, only needed if you want to build from the repository.
2. Download the DLB source code:
    1. Either from our website: [DLB Downloads][].
    2. Or from a git repository
        * Clone DLB repository
            * From GitHub:

                ```bash
                git clone https://github.com/bsc-pm/dlb.git
                ```
            * From our internal GitLab repository (BSC users only):

                ```bash
                git clone https://pm.bsc.es/gitlab/dlb/dlb.git
                ```
        * Or download from [GitHub releases][]
        * Bootstrap autotools:

            ```bash
            cd dlb
            ./bootstrap
            ```
3. Run `configure`. Optionally, check the configure flags by running
    `./configure -h` to see detailed information about some features.
    MPI support must be enabled with ``--with-mpi`` and, optionally,
    an argument telling where MPI can be located.

    ```bash
    ./configure --prefix=<DLB_PREFIX> [<configure-flags>]
    ```
4. Build and install

    ```bash
    make
    make install
    ```
5. Optionally, add the installed bin directory to your `PATH`

    ```bash
    export PATH=<DLB_PREFIX>/bin:$PATH
    ```

For more information about the autotools installation process,
please refer to [INSTALL](INSTALL)

## Basic usage

Choose between linking or preloading the binary with the DLB shared library
`libdlb.so` and configure DLB using the environment variable `DLB_ARGS`.

1. **Example 1:** Share CPUs between MPI processes

    ```bash
    # Link application with DLB
    mpicc -o myapp myapp.c -L<DLB_PREFIX>/lib -ldlb -Wl,-rpath,<DLB_PREFIX>/lib

    # Launch MPI as usual, each process will dynamically adjust the number of threads
    export DLB_ARGS="--lewi"
    mpirun -n <np> ./myapp
    ```

2. **Example 2:** Share CPUs between MPI processes with advanced affinity
control through OMPT.

    ```bash
    # Link application with an OMPT capable OpenMP runtime
    OMPI_CC=clang mpicc -o myapp myapp.c -fopenmp

    # Launch application:
    #   * Set environment variables
    #   * DLB library is preloaded
    #   * Run application with binary dlb_run
    export DLB_ARGS="--lewi --ompt"
    export OMP_WAIT_POLICY="passive"
    preload="<DLB_PREFIX>/lib/libdlb.so"
    mpirun -n <np> <DLB_PREFIX>/bin/dlb_run env LD_PRELOAD="$preload" ./myapp
    ```

3. **Example 3:** Manually reduce assigned CPUs to an OpenMP process.

    ```bash
    # Launch an application preloading DLB
    export OMP_NUM_THREADS=4
    export DLB_ARGS="--drom"
    export LD_PRELOAD=<DLB_PREFIX>/lib/libdlb.so
    taskset -c 0-3 ./myapp &

    # Reduce CPU binding to [1,3] and threads to 2
    myapp_pid=$!
    dlb_taskset -p $myapp_pid -c 1,3

    ```

4. **Example 4:** Get a TALP summary report at the end of an execution

    ```bash
    export DLB_ARGS="--talp"
    preload="<DLB_PREFIX>/lib/libdlb_mpi.so"
    mpirun <opts> env LD_PRELOAD="$preload" ./app
    ```

5. **Example 5:** Get a TALP summary report of a GPU application

    ```bash
    # Optional, only required if application is not MPI
    # otherwise, preload libdlb_mpi.so as usual
    export DLB_AUTO_INIT=1

    export DLB_ARGS="--talp --plugin=cupti"
    preload="<DLB_PREFIX>/lib/libdlb.so"
    env LD_PRELOAD="$preload" ./app
    ```


#### User Guide
Please refer to our [DLB User Guide][] for a more complete documentation.

## Citing DLB
If you want to cite DLB, you can use the following publications:

* [Hints to improve automatic load balancing with LeWI for hybrid applications][pub1_doi]
at Journal of Parallel and Distributed Computing 2014.
([bibtex][pub1_bib]) ([pdf][pub1_pdf])

* [LeWI: A Runtime Balancing Algorithm for Nested Parallelism][pub2_doi]
at International Conference in Parallel Processing 2009, ICPP09.
([bibtex][pub2_bib]) ([pdf][pub2_pdf])

* [DROM: Enabling Efficient and Effortless Malleability for Resource Managers][pub3_doi]
at 47th International Conference on Parallel Processing, August 2018.
([bibtex][pub3_bib]) ([pdf][pub3_pdf])

* [TALP: A Lightweight Tool to Unveil Parallel Efficiency of Large-scale Executions][pub4_doi]
at Performance EngineeRing, Modelling, Analysis, and VisualizatiOn STrategy, June 2021
([bibtex][pub4_bib]) ([pdf][pub4_pdf])


## Contact Information

For questions, suggestions and bug reports, you can contact us via e-mail
at dlb@bsc.es.

[DLB]: https://pm.bsc.es/dlb
[DLB Downloads]: https://pm.bsc.es/dlb-downloads
[GitHub releases]: https://github.com/bsc-pm/dlb/releases
[DLB User Guide]: https://pm.bsc.es/ftp/dlb/doc/user-guide/index.html
[pub1_doi]: http://dx.doi.org/10.1016/j.jpdc.2014.05.004
[pub1_bib]: https://pm.bsc.es/ftp/dlb/doc/LeWI_JPDC14.bib
[pub1_pdf]: https://pm.bsc.es/ftp/dlb/doc/JPDC_2014.pdf
[pub2_doi]: http://dx.doi.org/10.1109/ICPP.2009.56
[pub2_bib]: https://pm.bsc.es/ftp/dlb/doc/LeWI_ICPP09.bib
[pub2_pdf]: https://pm.bsc.es/ftp/dlb/doc/LeWI_ICPP09.pdf
[pub3_doi]: https://doi.org/10.1145/3229710.3229752
[pub3_bib]: https://pm.bsc.es/ftp/dlb/doc/2018_DROM.bib
[pub3_pdf]: https://pm.bsc.es/ftp/dlb/doc/drom_preprint.pdf
[pub4_doi]: https://doi.org/10.1145/3452412.3462753
[pub4_bib]: https://pm.bsc.es/ftp/dlb/doc/TALP_PERMAVOST21.bib
[pub4_pdf]: https://pm.bsc.es/ftp/dlb/doc/TALP_PERMAVOST21.pdf
