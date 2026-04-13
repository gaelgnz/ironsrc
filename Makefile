CC      = clang
CFLAGS  = -Wall -Wextra -O3 -I./include -I./enet/include
LDFLAGS = -lraylib -lGL -lm -lpthread -lrt -lX11
LDFLAGS_SERVER = -lm -lpthread -lrt
BIN     = ironsrc
SERVER  = server
SRCS = $(wildcard src/*.c) $(wildcard enet/*.c)
OBJS = $(patsubst %.c, obj/%.o, $(notdir $(SRCS)))

SERVER_SRCS = src/server.c $(wildcard enet/*.c)
SERVER_OBJS = $(patsubst %.c, obj/%.o, $(notdir $(SERVER_SRCS)))

all: $(BIN)
	./$(BIN)

server: $(SERVER)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

$(SERVER): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $(SERVER) $(LDFLAGS_SERVER)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: enet/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/server.o: src/server.c | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj:
	mkdir -p obj

clean:
	rm -rf obj $(BIN) $(SERVER)

.PHONY: all clean server
