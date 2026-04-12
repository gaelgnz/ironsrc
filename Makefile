CC      = clang
CFLAGS  = -Wall -Wextra -O3 -I./include -I./enet/include
LDFLAGS = -lraylib -lGL -lm -lpthread -lrt -lX11

BIN     = ironsrc

SRCS = $(wildcard src/*.c) $(wildcard enet/*.c)
OBJS = $(patsubst %.c, obj/%.o, $(notdir $(SRCS)))

all: $(BIN)
	./$(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: enet/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

clean:
	rm -rf obj $(BIN)

.PHONY: all clean
