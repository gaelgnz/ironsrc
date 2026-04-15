CC = clang
CFLAGS = -Wall -Wextra -Iraylib/src -Isrc
LDFLAGS = -Lraylib/src -lraylib -lm -lpthread -ldl -lrt -lX11 -lglfw

BUILD_DIR = build
SRC_DIR = src

SRCS = $(filter-out $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all server clean

all: ironsrc

ironsrc: $(OBJS)
	$(CC) $(OBJS) -o ironsrc $(LDFLAGS)

server: $(BUILD_DIR)/server.o
	$(CC) $(BUILD_DIR)/server.o -o server $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ironsrc server
