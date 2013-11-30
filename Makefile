CC = gcc
CFLAGS = -Werror -Wall -ansi -pedantic -g

OBJS = main.o

build: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o sim
run: build
	./sim
main.o: main.c
	$(CC) $(CFLAGS) -c main.c
clean:
	rm -rf *.o
