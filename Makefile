CC = clang
CFLAGS = -O3 -Wall -g -Wextra -Iraylib/src -Isrc
LDFLAGS = -Lraylib/src \
          raylib/src/libraylib.a \
          -lm -lpthread -ldl -lrt \
          -lX11 -lXrandr -lXinerama -lXi -lXcursor \
          -lglfw \
          -static-libgcc

BUILD_DIR = build
SRC_DIR = src

CLIENT_SRCS = $(filter-out $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))
CLIENT_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))

SERVER_SRCS = $(filter-out $(SRC_DIR)/main.c,$(wildcard $(SRC_DIR)/*.c))
SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

.PHONY: all ironsrc server clean raylib

all: ironsrc

# Build raylib static lib if not already built
raylib/src/libraylib.a:
	$(MAKE) -C raylib/src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC CC=$(CC)

ironsrc: raylib/src/libraylib.a $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o ironsrc $(LDFLAGS)

server: raylib/src/libraylib.a $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o server $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ironsrc server

clean-all: clean
	$(MAKE) -C raylib/src clean
