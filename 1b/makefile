# @file Makefile
# @author Tobias Scharsching e12123692@student.tuwien.ac.at
# @date 11.11.2022
#
# @brief Makefile for supervisor and generator

CC = gcc -g
DEFS = -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)
LDFLAGS = -lrt -pthread


.PHONY: all clean 
all: generator supervisor

generator: generator.o
	$(CC) $(LDFLAGS) -o $@ $^

supervisor: supervisor.o
	$(CC) $(LDFLAGS) -o $@ $^
	

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o generator supervisor