CC = gcc
CFLAGS = -Wall -g -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS = -L. -lgame -lpthread

# Source files for library
LIB_SRCS = proto.c logging.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: libgame.a server client

# Static library containing protocol and logging modules
libgame.a: $(LIB_OBJS)
	ar rcs libgame.a $(LIB_OBJS)
	@echo "Built static library: libgame.a"

proto.o: proto.c proto.h common.h
	$(CC) $(CFLAGS) -c proto.c

logging.o: logging.c logging.h
	$(CC) $(CFLAGS) -c logging.c

server: server.c libgame.a common.h proto.h logging.h
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)
	@echo "Built server executable"

client: client.c libgame.a common.h proto.h logging.h
	$(CC) $(CFLAGS) client.c -o client $(LDFLAGS)
	@echo "Built client executable"

# Run stress test
stress: client server
	@echo "Starting stress test..."
	./client -stress 100

# Clean build artifacts
clean:
	rm -f *.o *.a server client

# Show help
help:
	@echo "Available targets:"
	@echo "  all     - Build everything (default)"
	@echo "  server  - Build server only"
	@echo "  client  - Build client only"
	@echo "  stress  - Run stress test with 100 clients"
	@echo "  clean   - Remove build artifacts"
	@echo ""
	@echo "Usage:"
	@echo "  Server: ./server"
	@echo "  Client: ./client"
	@echo "  Stress: ./client -stress [num_clients]"

.PHONY: all clean stress help
