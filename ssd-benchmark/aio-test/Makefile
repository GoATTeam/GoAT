.PHONY: all clean

all: aio-test aio-example

CC=gcc
CFLAGS= -Wall -Iccan/ -ggdb3 -O2 -D_GNU_SOURCE

aio-example: aio-example.c
	$(CC) $(CFLAGS) $< -o $@

aio-test: aio-test.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f aio-example aio-test
