# Dynamic Load Balancing Library

DLB is a dynamic library designed to speed up HPC hybrid applications (i.e.,
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


## Installation

1. Build requirements
    * A supported platform running Linux (i386, x86-64, ARM, PowerPC or IA64)
    * GNU C/C++ compiler versions 4.4 or higher
    * Python 2.4 or higher
2. Download DLB code
    1. From a git repository
        * Clone DLB repository
            * From GitHub:

                ```bash
                git clone https://github.com/bsc-pm/dlb.git
                ```
            * From our internal GitLab repository (BSC users only):

                ```bash
                git clone https://pm.bsc.es/gitlab/dlb/dlb.git
                ```
        * Bootstrap autotools:

            ```bash
            cd dlb
            ./bootstrap
            ```
    2. From a distributed tarball
        * Download a tarball from [DLB Downloads][] or [GitHub releases][]
3. Run `configure`. Optionally, check the configure flags by running
    `./configure -h` to see detailed information about some features.

    ```bash
    ./configure --prefix=<DLB_PREFIX> [<configure-flags>]
    ```
4. Build ans install

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

Simply link or preload a binary with the DLB shared library `libdlb.so` and
configure DLB using the environment variable `DLB_ARGS`.

```bash
# Link application
mpicc -o myapp myapp.c -L<DLB_PREFIX>/lib -ldlb -Wl,-rpath,<DLB_PREFIX>/lib

# Launch two processes sharing resources
export DLB_ARGS="--lewi"
mpirun -n 2 ./myapp
```

```bash
# Launch an application preloading DLB
export OMP_NUM_THREADS=4
export LD_PRELOAD=<DLB_PREFIX>/lib/libdlb.so
taskset -c 0-3 ./myapp &

# Reduce CPU binding to [1,3] and threads to 2
myapp_pid=$!
dlb_taskset -p $myapp_pid -c 1,3
```

#### User Guide
Please refer to our [DLB User Guide][] for a more complete documentation.

## Citing DLB
If you want to cite DLB, you can use the following publications:

* [Hints to improve automatic load balancing with LeWI for hybrid applications][pub1_pdf]
at Journal of Parallel and Distributed Computing 2014.
([ScienceDirect][pub1_ref]) ([bibtex][pub1_bib]) ([pdf][pub1_pdf])

* [LeWI: A Runtime Balancing Algorithm for Nested Parallelism][pub2_pdf]
at International Conference in Parallel Processing 2009, ICPP09.
([IEEE Xplore][pub2_ref]) ([bibtex][pub2_bib]) ([pdf][pub2_pdf])


## Contact Information

For questions, suggestions and bug reports, you can contact us via e-mail
at pm-tools@bsc.es.

[DLB Downloads]: https://pm.bsc.es/dlb-downloads
[GitHub releases]: https://github.com/bsc-pm/dlb/releases
[DLB User Guide]: https://pm.bsc.es/dlb-docs/user-guide/index.html
[pub1_pdf]: https://pm.bsc.es/sites/default/files/ftp/dlb/doc/JPDC_2014.pdf
[pub1_ref]: http://www.sciencedirect.com/science/article/pii/S0743731514000926
[pub1_bib]: https://pm.bsc.es/sites/default/files/ftp/dlb/doc/LeWI_JPDC14.bib
[pub2_pdf]: https://pm.bsc.es/sites/default/files/ftp/dlb/doc/LeWI_ICPP09.pdf
[pub2_ref]: http://ieeexplore.ieee.org/xpl/articleDetails.jsp?tp=&arnumber=5362480
[pub2_bib]: https://pm.bsc.es/sites/default/files/ftp/dlb/doc/LeWI_ICPP09.bib
