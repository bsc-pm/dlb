
#commmon32.o : common.c
#	$(CC) $(CFLAGS) -qsmp=omp -qthreaded $(FLAGS32) -c -o $@ $<

#common.o : common.c
#	$(CC) $(CFLAGS) -qsmp=omp -qthreaded $(FLAGS64) -c -o $@ $<

%32.o : %.c
	$(CC) $(CFLAGS) $(FLAGS32) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(FLAGS64) -c -o $@ $<


clean:
	rm -rf *.o
