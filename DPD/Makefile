
%32.o : %.c
	$(CC) $(CFLAGS) $(FLAGS32) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) $(FLAGS64) -c -o $@ $<


clean:
	rm -rf *.o
