%32.o : %.c
	$(MPICC) $(CFLAGS) $(FLAGS32) -c -o $@ $<

%.o : %.c
	$(MPICC) $(CFLAGS) $(FLAGS64) -c -o $@ $<


clean:
	rm -rf *.o
