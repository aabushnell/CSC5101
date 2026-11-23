CC = gcc
CFLAGS = -Wall -O2

all: queue radix

queue: queue.c
	$(CC) $(CFLAGS) -o $@ queue.c

Q_LOCK: queue
	./queue 0

Q_FREE: queue
	./queue 1

radix: radix.c
	$(CC) $(CFLAGS) -o $@ radix.c

R: radix
	./radix

clean:
	rm -f queue
