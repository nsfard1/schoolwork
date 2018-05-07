CC = gcc 

CFLAGS =-Wall -fPIC -g

liblwp.so: magic64.o lwp.o
	$(CC) $(CFLAGS) -shared -o $@ lwp.o magic64.o

lwp.o: lwp.c
	$(CC) $(CFLAGS) -c  lwp.c

magic64.o: magic64.S
	$(CC) -c -o magic64.o magic64.S

clean: 
	rm liblwp.so lwp.o
