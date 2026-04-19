CC = clang

CFLAGS = -O3 -Wall -g -Wextra -Iraylib/src -Isrc 

LDFLAGS = -Lraylib/src -lraylib -lm -lpthread -ldl -lrt -lX11 -lglfw

BUILD_DIR = build
SRC_DIR = src

CLIENT_SRCS = $(filter-out $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))


CLIENT_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_SRCS = $(filter-out $(SRC_DIR)/main.c,$(wildcard $(SRC_DIR)/*.c))
SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

.PHONY: all ironsrc server clean

all: ironsrc

ironsrc: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o ironsrc $(LDFLAGS)

server: $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o server $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ironsrc server
