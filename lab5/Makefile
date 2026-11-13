CC = gcc
CFLAGS = -Wall -O2 -march=native

all: nvmm

nvmm: main.c
	$(CC) $(CFLAGS) -o $@ main.c

run: nvmm
	./nvmm

clean:
	rm -f nvmm
