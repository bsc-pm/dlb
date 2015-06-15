
MPICC = OMPI_CC="smpcc --ompss --instrument" mpicc

mpi_ompss_pils: mpi_ompss_pils.c
	$(MPICC) $^ -o $@

.PHONY: clean
clean:
	rm -f mpi_ompss_pils
