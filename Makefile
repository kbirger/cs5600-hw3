#
# file:        hw3/Makefile
# description: Makefile for CS 5600 Homework 3
#
# Peter Desnoyers, Northeastern CCIS, 2009
# $Id: Makefile 70 2009-12-01 02:21:48Z pjd $
#

CFLAGS = -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -pthread -g -Wall
LDFLAGS = -L/lib -lfuse -lrt -ldl

EXES = cs5600-hw3 mkfs-hw3

all: $(EXES)

cs5600-hw3: cs5600-hw3.o cs5600-misc.o 

mkfs-hw3: mkfs-hw3.o

clean:
	rm -f *.o *~ $(EXES)
