CC = gcc

CFLAGS = -Wall -ansi -pedantic

hello: hello.o
	$(CC) -g -o hello hello.o

test: hello.o
	$(CC) -g -o hello hello.o
	@echo "Testing hello..."
	@./hello
	@echo "done."

hello.o: hello.c
	$(CC) $(CFLAGS) -g -c -o hello.o hello.c

clean:
	rm -f hello.o *~ TAGS
