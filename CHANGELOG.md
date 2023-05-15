# Change Log
All notable changes to this project will be documented in this file

The format is based on [Keep a Changelog](http://keepachangelog.com/).

## [Unreleased]
### Added
- Free agent and Role-shift OMPT thread managers to support LeWI with both
  implementations
- Flag `--ompt-thread-manager` to select which OpenMP implementation to use
- MPI Fortran 2008 bindings
- TALP flag to generate file in different output formats `--talp-output-file`
- New TALP collective functions to gather and compute metrics:
  `DLB_TALP_CollectPOPMetrics` and `DLB_TALP_CollectNodeMetrics`

### Changed
- `libdlb_mpi.so` and `libdlb_mpi_*.so` have now both C and Fortran MPI symbols

### Fixed
- Fixed DROM pre-initialization if child had empty cpuset affinity
- Fixed `--lewi-max-parallelism`
- Fixed several TALP bugs
- Fixed some finalization errors during MPI finalize
- Fixed cpuset parsing when provided a non-contiguous mask

## [3.2] 2022-03-16
### Added
- Flag `--verbose` to enable all verbose modes
- Flag `--talp-summary=pop-raw` to print raw POP metrics
- Flag `--lewi-respect-cpuset` to allow LeWI to use CPUs not yet registered

### Changed
- DROM can now steal all CPUs from one process
- DROM can now inherit a subset of CPUs from other process
- `DLB_DROM_SetProcessMask` to oneself does not longer require a `DLB_pollDROM`
- `DLB_Lend` in OpenMP applications now invokes the OpenMP runtime to change
  the number of threads

### Fixed
- Fixed TALP regions enabled or registered only on some processes
- Fixed minor option parsing

## [3.1] 2021-11-05
### Added
- New `--lewi-mpi-calls` value: `none`
- New MPI runtime version check during initialization
- Experimental meson build files
- Add better support for getting/setting process mask from own process

### Changed
- Enable `--barrier` by default
- Rename `--lewi-mpi` to its opposite: `--lewi-keep-one-cpu`, and
  change the default behavior
- Rename `--talp-summary=app` to `--talp-summary=pop-metrics` and
  make it the default value
- Rename TALP to Tracking Application Live Performance
- CPU priority now follows a better topology order
- Properly clean up shared memory during initialization

### Fixed
- Fixed several TALP issues
- Improves support for OMPT
- Proper shared memory clean up if running under `dlb_run`

### Deprecated
- Python viewer scripts are no longer installed

## [3.0] 2021-01-15
### Added
- New TALP module: Tracking Application Low-level Performance
- TALP Monitoring Regions for user-defined regions
- Allow processes to attach to / detach from the Barrier module
- Improve the verbose messages for some modules
- Allow partial instrumentation of some events
- Man pages for DLB commands

### Changed
- DLB library now always prints to stderr, DLB binaries may still use stdout
- Dropped support for binary mask old format `1000b` in favor of `0b0001`
- `DLB_ARGS` variable now takes precedence over `DLB_Init` argument

### Fixed
- Some callbacks not being invoked when the action involved some successful
  actions and some others not allowed
- Several DROM inconsistencies
- Several minor fixes
- Minor documentation fixes

## [2.1] 2019-07-15
### Added
- OMPT full support
- Add function to API: `DLB_UnsetMaxParallelism`
- New binary `dlb_run` to preinit masks, needed for OMPT applications
- Improved `dlb_shm --list` output
- New verbose option `affinity` to print hardware information
- Add option `--quiet` to silent all info and warning messages
- New test mechanism based on LIT

### Changed
- `DLB_Lend` does no longer keeps the current CPU
- PreInit service now handles the timeout if the synchronous flag is provided
- DROM now accepts registering and setting empty masks
- Verbose options now affect all library versions

### Fixed
- Added some mechanisms to clean shared memories when the program aborts
- Fixed several race conditions with the asynchronous messages
- Fixed an issue where `--lewi-affinity` was being ignored
- Adapt Fortran headers to be fixed-form compatible

## [2.0.2] 2018-10-29
### Fixed
- `DLB_ARGS` was not being correctly parsed in some cases
- DROM API contained a typo. New function is called `DLB_DROM_Detach`
- Intel compiler and GCC8 compatibility
- Several minor bugs

## [2.0.1] 2018-02-27
### Fixed
- MaxParallelism is updated correctly when set and later decreased
- AcquireCpus now correctly ignores reclaimed CPUs
- Visualization errors parsing `DLB_ARGS`
- Removed non intended warning about setting CPUs when using LeWI with MPI support
- Several minor fixes

## [2.0] 2017-12-21
### Added
- Callback system
- API for subprocesses
- New interaction mode choice (synchronous as it was before, or asynchronous)
- Doxygen HTML and man pages documentation
- Test coverage support
- OMPT experimental support

### Changed
- API reworked
- A petition of a CPU now registers the process into a CPU petition queue. If this CPU
  becomes available, DLB can schedule which process will acquire it
- A petition of an unspecified CPU registers the process into a global petition queue,
  this queue has less priority than the CPU queue
- DLB acquire now does not schedule because it forces the acquisition, DLB Borrow does
  scheduling but only looks for idle CPUs and never creates a CPU request
- DLB options print format reworked, `DLB_ARGS` is now used to pass options to DLB.
- Fortran Interface is now provided using an include with ISO C bindings.
- Shared Memory synchronization mechanism is now managed using a pthread spinlock
- DROM services now use the same shmem handler so no need to call `DLB_Init` and `DLB_DROM_Init`
- DROM services Get/Set process mask are now asynchronous functions

### Fixed
- Test suite now follows a Unit Testing approach and does not need undocumented external tools
- Several minor bugs
- PreInit service now correctly handles environment variables and allows forks

### Deprecated
- `DLB_MASK` is no longer used, only registered CPUs may be used by other processes
- `DLB_DROM_GetCpus` service has been removed
- `LB_OPTION` type of environment variables is deprecated

## [1.3.2] - 2017-10-9
### Fixed
- Bug initialing shared memory when using non-mask policies

## [1.3.1] - 2017-10-4
### Fixed
- Bug parsing `DLB_ARGS`

## [1.3] - 2017-07-12
### Added
- This CHANGELOG file
- Shared Memory consistency checks upon registration
- The binary `dlb_taskset` can now be used to launch processes under DLB
- Service `DLB_Barrier` to perform a barrier between all processes in the node
- DLB Services now return an error code
- New User Option `LB_PRIORITY` to manage how CPUs are distributed according to HW locality
- Add services for DROM to PreInit and PostFinalize to manage process that will fork/exec
- Add services to check if the process mask needs to be changed

### Changed
- Refactor policy based Shared Memory into two general purpose `cpuinfo` and `procinfo`
- DROM interface is now considered stable. External processes can manage the CPU ownership of DLB processes
- DLB options are now parsed through the environment variable `DLB_ARGS`

### Fixed
- Several minor bugs

### Deprecated
- Signal handler feature is no longer supported

## [1.2] - 2016-12-23
### Changed
- Improved User Guide
- Improved configure script and macros

### Fixed
- Compatibility with gcc6
- Compatibility with icc17
- Compatibility with C11 standard
- Several minor bugs

## [1.1] - 2016-02-04
### Added
- Policy `Autonomous Lewi Mask` to micro-manage each thread individually
- DLB services to init/finalize without MPI support
- DLB services to enable/disable or mark a serial/parallel sections of code
- DLB service to modify User Options during program execution
- New library independent from MPI
- Binary `dlb_shm` to manage Shared Memory from CLI
- Binary `dlb_taskset` to manage processes' mask from CLI
- BG/Q support
- Add `LB_MASK` environment variable to specify a `DLB_MASK` common to all processes
- Experimental implementation of `DLB_Stats` interface
- Experimental implementation of `DLB_DROM` interface
- Signal handler feature to clean up DLB on termination
- In-code Doxygen documentation
- User guide using Sphinx generator
- Distributable examples

### Changed
- Replace IPC with POSIX Shared Memory
- Replace Shared Memory synchronization mechanism from semaphores to pthread mutexes
- Shared Memory creation is now decentralized from rank 0 and it will be created when necessary
- Shared Memory internal structure reworked to host information about ownership and guesting of CPUs
- CPUs from `DLB_MASK` can guest any process, but they will not have an owner
- Improve support for detection of Shared Memory runtimes

### Fixed
- Compatibility with gcc 4.4 and 4.5
- Compatibility with MPI-3 standard
- Fortran interfaces now use the correct types
- Several minor bugs

## 1.0 - 2013-12-11
### Added
- DLB Interface
- MPI Interception Interface
- User options configured by environment variables `LB_*`
- Lend When Idle algorithm implementation
- DPD
- Extrae instrumentation
- Paraver configs for DLB events
- Inter-Process Communication (IPC) Shared Memory
- Inter-Process Communication (IPC) Sockets
- Thread binding interface
- `LeWI` policies to support SMPSS and OpenMP
- `LeWI mask` policy to support OmpSs
- RaL and PERaL policies for redistribution of resources
- HWLOC optional dependency to query HW info
- Scheduling decisions based on HW locality
- Binary `dlb`

[Unreleased]: https://github.com/bsc-pm/dlb/compare/v3.2...HEAD
[3.2]: https://github.com/bsc-pm/dlb/compare/v3.1...v3.2
[3.1]: https://github.com/bsc-pm/dlb/compare/v3.0...v3.1
[3.0.2]: https://github.com/bsc-pm/dlb/compare/v3.0.1...v3.0.2
[3.0.1]: https://github.com/bsc-pm/dlb/compare/v3.0...v3.0.1
[3.0]: https://github.com/bsc-pm/dlb/compare/v2.1...v3.0
[2.1]: https://github.com/bsc-pm/dlb/compare/v2.0...v2.1
[2.0.2]: https://github.com/bsc-pm/dlb/compare/v2.0.1...v2.0.2
[2.0.1]: https://github.com/bsc-pm/dlb/compare/v2.0...v2.0.1
[2.0]: https://github.com/bsc-pm/dlb/compare/v1.3...v2.0
[1.3.2]: https://github.com/bsc-pm/dlb/compare/v1.3.1...v1.3.2
[1.3.1]: https://github.com/bsc-pm/dlb/compare/v1.3...v1.3.1
[1.3]: https://github.com/bsc-pm/dlb/compare/v1.2...v1.3
[1.2]: https://github.com/bsc-pm/dlb/compare/v1.1...v1.2
[1.1]: https://github.com/bsc-pm/dlb/compare/v1.0...v1.1
