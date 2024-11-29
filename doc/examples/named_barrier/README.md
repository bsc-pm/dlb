README
======

This is an example of two coupled applications synchronising with DLB_Barrier
while both applications are running overlapped in the same resources.

Compilation
-----------
Check that the Makefile paths are correct and run `make`.

If you want to try the coupled applications without DLB, edit the Makefile,
comment out the definition of the DLB variable, and compile again.

Execution
---------
Open the `run.sh` file and edit any flag options you need. The script assumes
OpenMPI and an 8 CPU system where hardware threads are treated as CPUs. You
may need to change the number of ranks or other MPI options depending on your
system.

Launch the `run.sh` script to execute both applications. They will run in
rotation; the first application will do some computation while the second one
waits for the result, which will be passed via a file. After that, the first
application blocks while the second computes, and so on.
