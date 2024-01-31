CC=gcc
CFLAGS=-Wall -Werror -Wno-unused-function -Wextra -g3 -fsanitize=undefined -fno-sanitize-recover

default: rlsmenu.o

rlsmenu.o: rlsmenu.c rlsmenu.h
	$(CC) -c -o $@ $< $(CFLAGS)

demo: demo.c rlsmenu.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f demo *.o
