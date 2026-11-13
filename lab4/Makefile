CC = gcc
CFLAGS = -Wall -O2

all: tm

tm: main.c
	$(CC) $(CFLAGS) -o $@ main.c

run: tm
	./tm 4

clean:
	rm -f tm
