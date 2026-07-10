
DLB = -I$(DLB_HOME)/include -L$(DLB_HOME)/lib -ldlb -Wl,-rpath,$(DLB_HOME)/lib

BINARIES = \
	mpi/test_mpi        \
	omp/test_omp        \
	omp/test_omp_nested \
	hybrid/test_hybrid

all: $(BINARIES)

mpi/test_mpi: mpi/test_mpi.c
	mpicc -O0 $^ -o $@

omp/test_omp: omp/test_omp.c
	clang -fopenmp -O0 $^ -o $@

omp/test_omp_nested: omp/test_omp_nested.c
	clang -fopenmp -O0 $^ -o $@ $(DLB)

hybrid/test_hybrid: hybrid/test_hybrid.c
	OMPI_CC=clang mpicc -fopenmp -O0 $^ -o $@ $(DLB)

.PHONY: clean
clean:
	rm -f $(BINARIES)
