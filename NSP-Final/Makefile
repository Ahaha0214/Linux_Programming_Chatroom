CC = gcc
CFLAGS = -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS = -L. -lgame -lpthread

all: libgame.a server client

libgame.a: proto.o
	ar rcs libgame.a proto.o

proto.o: proto.c proto.h common.h
	$(CC) $(CFLAGS) -c proto.c

server: server.c libgame.a common.h
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)

client: client.c libgame.a common.h
	$(CC) $(CFLAGS) client.c -o client $(LDFLAGS)

clean:
	rm -f *.o *.a server client
