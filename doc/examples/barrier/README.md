README
======

This is a small example of the DLB_Barrier function.

Compilation
-----------
Check that the Makefile paths are correct and run `make`.

Note that two binaries are generated: `app` and `app_barrier`. The latter is
linked with `-ldlb_instr` for the sake of the example. If tracing is not
intended you may safely use `-ldlb`.

Execution
---------
Open the `run.sh` file and edit any flag options you need. The script assumes
OpenMPI and an 8 CPU system where hardware threads are treated as CPUs. You
may need to change the number of ranks or other MPI options depending on your
system.

Launch the `run.sh` script to execute an emulation of a coupled application.
Only processes with the same coupling id (the first parameter) will compute at
the same time. When all of them reach the barrier, the other processes will
start.

For tracing, edit the variable TRACE=1, make sure EXTRAE_HOME is set and run as
usual.
