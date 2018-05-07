# Makefile for CPE464 library
#
# You should modify this makefile to meet your needs and to meet the
# requirements of the assignment.  This makefile is only provided as
# an example.  You can use it as much as you want but using this makefile
# is not required.

CC = gcc
CFLAGS = -g -Wall -Werror

OS = $(shell uname -s)
ifeq ("$(OS)", "Linux")
	LIBS1 = -lstdc++
endif

LIBS += -lstdc++

SRCS = $(shell ls *.cpp *.c 2> /dev/null)
OBJS = $(shell ls *.cpp *.c 2> /dev/null | sed s/\.c[p]*$$/\.o/ )
LIBNAME = $(shell ls *cpe464*.a)

ALL = rcopy server

all: $(OBJS) $(ALL)

lib:
	make -f lib.mk

echo:
	@echo "Objects: $(OBJS)"
	@echo "LIBNAME: $(LIBNAME)"

.cpp.o:
	@echo "-------------------------------"
	@echo "*** Building $@"
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS1)

.c.o:
	@echo "-------------------------------"
	@echo "*** Building $@"
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS1)

rcopy: rcopy.c 
	@echo "-------------------------------"
	@echo "*** Linking $@ with library $(LIBNAME)... "
	$(CC) $(CFLAGS) -o $@ $^ networks.c srej.c $(LIBNAME) $(LIBS)
	@echo "*** Linking Complete!"
	@echo "-------------------------------"

server: server.c
	@echo "-------------------------------"
	@echo "*** Linking $@ with library $(LIBNAME)... "
	$(CC) $(CFLAGS) -o $@ $^ networks.c srej.c $(LIBNAME) $(LIBS)
	@echo "*** Linking Complete!"
	@echo "-------------------------------"

test: test.c
	@echo "-------------------------------"
	@echo "*** Linking $@ with library $(LIBNAME)... "
	$(CC) $(CFLAGS) -o $@ $^ $(LIBNAME) $(LIBS)
	@echo "*** Linking Complete!"
	@echo "-------------------------------"

# clean targets for Solaris and Linux
clean: 
	@echo "-------------------------------"
	@echo "*** Cleaning Files..."
	rm -f *.o $(ALL)
	@echo "-------------------------------"
