# @file Makefile
# @author Tobias Scharsching e12123692@student.tuwien.ac.at
# @date 11.11.2022
#
# @brief Makefile for main

CC = gcc -g
DEFS = -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

OBJECTS = main.o

.PHONY: all clean
all: myexpand

myexpand: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o: main.c

clean:
	rm -rf *.o myexpand