CC ?= gcc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra
CPPFLAGS ?= -I../src
LDFLAGS ?=
SERVER_BIN ?= relay_server

$(SERVER_BIN): relay_server.c ../src/cJSON.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ relay_server.c ../src/cJSON.c $(LDFLAGS)

.PHONY: all clean

all: $(SERVER_BIN)

clean:
	rm -f relay_server $(SERVER_BIN)
