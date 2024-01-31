# IDIR=include
# SDIR=src
# TDIR=test
CC=gcc
# CFLAGS=-I$(IDIR) -Wall
CFLAGS=-Wall -Werror -Wno-unused-function -Wextra -g3 -fsanitize=undefined -fno-sanitize-recover

default: rlsmenu.o

rlsmenu.o: rlsmenu.c rlsmenu.h
	$(CC) -c -o $@ $< $(CFLAGS)

demo: rlsmenu.c rlsmenu.h
	$(CC) -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f demo *.o
