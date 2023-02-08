CC = gcc
CFLAGS = -Wall -lpthread

.PHONY: all clean

CC = gcc
CFLAGS = -Wall

all: server

server: main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm *.o

