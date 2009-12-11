
%32.o : %.c
	$(CC) $(CFLAGS) -q32 -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -q64 -c -o $@ $<


clean:
	rm -rf *.o
