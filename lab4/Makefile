CC = gcc
CFLAGS = -Wall -O2

all: tm

tm: main.c
	$(CC) $(CFLAGS) -o $@ main.c

STM: tm
	./tm 0

STM_BACK: tm
	./tm 1

LOCK: tm
	./tm 2

ATOMIC: tm
	./tm 3

clean:
	rm -f tm
