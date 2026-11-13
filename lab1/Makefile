
all: exc1 exc2 exc3

exc1: exc1.o
	gcc -g -o exc1 exc1.o

exc1.o: exc1.c
	gcc -c -Wall -g exc1.c

exc2: exc2.o
	gcc -g -o exc2 exc2.o

exc2.o: exc2.c
	gcc -c -Wall -g exc2.c

exc3: exc3.o
	gcc -g -o exc3 exc3.o

exc3.o: exc3.c
	gcc -c -Wall -g exc3.c

clean:
	rm -f exc1 exc1.o
	rm -f exc2 exc2.o
	rm -f exc3 exc3.o

