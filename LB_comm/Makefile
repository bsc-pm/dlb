%32.o : %.c
	$(MPICC) $(CFLAGS) -q32 -c -o $@ $<

%.o : %.c
	$(MPICC) $(CFLAGS) -q64 -c -o $@ $<


clean:
	rm -rf *.o
