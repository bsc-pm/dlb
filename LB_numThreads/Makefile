
#numThreads32.o : numThreads.c
#	$(CC) $(CFLAGS) $(FLAGS32) -fopenmp -c -o $@ $<

#numThreads.o : numThreads.c
#	$(CC) $(CFLAGS) $(FLAGS64) -fopenmp -c -o $@ $<

%32.o : %.c
	$(CC) $(CFLAGS) $(FLAGS32) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(FLAGS64) -c -o $@ $<


clean:
	rm -rf *.o
