CC = gcc

CFLAGS = -Wall -fPIC -g

mksuid: mksuid.c
	$(CC) $(CFLAGS) -o mksuid mksuid.c -lcrypt

clean: 
	rm mksuid
