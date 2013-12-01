CC = gcc
CFLAGS = -Werror -Wall -ansi -pedantic -g -O0

OBJS = main.o

build: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o sim
run: build
	./sim > test_output.txt
	diff test_output.txt output.txt
main.o: main.c
	$(CC) $(CFLAGS) -c main.c
clean:
	rm -rf *.o
