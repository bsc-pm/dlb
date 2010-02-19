%32.o : %.c
	$(MPICC) -cc=gcc $(CFLAGS) $(FLAGS32) -c -o $@ $<

%.o : %.c
	$(MPICC) -cc=gcc $(CFLAGS) $(FLAGS64) -c -o $@ $<


clean:
	rm -rf *.o
