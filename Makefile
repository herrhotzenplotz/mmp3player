PKGS = libpulse-simple

CC ?= gcc
CFLAGS = -Wall -Wextra -Werror -fno-builtin -O2 $(shell pkg-config --cflags $(PKGS)) -ggdb

LIBS=$(shell pkg-config --libs $(PKGS))

all: mmp3player Makefile

.PHONY: all

mmp3player: main.o
	$(CC) $< $(LIBS) -o $@

seek: seek.o
	$(CC) $< $(LIBS) -o $@

seek.o: seek.c
	$(CC) -c $(CFLAGS) $< -o $@

main.o: main.c
	$(CC) -c $(CFLAGS) $< -o $@
