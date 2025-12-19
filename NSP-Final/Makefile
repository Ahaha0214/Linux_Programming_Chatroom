CC = gcc
CFLAGS = -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS = -L. -lgame -lpthread

all: libgame.a server client

libgame.a: proto.o
	ar rcs libgame.a proto.o

proto.o: proto.c proto.h common.h
	$(CC) $(CFLAGS) -c proto.c

## 新增 ##
LIB_SRCS = proto.c logging.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: libgame.a server client

libgame.a: $(LIB_OBJS)
	ar rcs libgame.a $(LIB_OBJS)
	@echo "Built static library: libgame.a"

proto.o: proto.c proto.h common.h
	$(CC) $(CFLAGS) -c proto.c

logging.o: logging.c logging.h
	$(CC) $(CFLAGS) -c logging.c

#  help 和 stress targets
stress: client server
	./client -stress 100

help:
	@echo "Usage: ..."

.PHONY: all clean stress help
## 新增 ##

server: server.c libgame.a common.h
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)

client: client.c libgame.a common.h
	$(CC) $(CFLAGS) client.c -o client $(LDFLAGS)

clean:
	rm -f *.o *.a server client
