# Change Log
All notable changes to this project will be documented in this file

The format is based on [Keep a Changelog](http://keepachangelog.com/).

## [Unreleased]
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
- User guide using Sphynx generator
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
- Fotran interfaces now use the correct types
- Several minor bugs

## [1.0] - 2013-12-11
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
