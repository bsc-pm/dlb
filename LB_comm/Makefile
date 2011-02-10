%32-mpi2.o : %.c
	$(CC) $(CFLAGS) $(FLAGS32) $(MPI2INCLUDE) -c -o $@ $<

%-mpi2.o : %.c
	$(CC) $(CFLAGS) $(FLAGS64) $(MPI2INCLUDE) -c -o $@ $<

%32.o : %.c
	$(CC) $(CFLAGS) $(FLAGS32) $(MPIINCLUDE) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(FLAGS64) $(MPIINCLUDE) -c -o $@ $<


clean:
	rm -rf *.o
