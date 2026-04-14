CC = clang
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lraylib -lm -ldl -lpthread -lX11

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TARGET = app
SERVER = server

# remove server.o from normal build
OBJS_NO_SERVER = $(filter-out $(OBJ_DIR)/server.o, $(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS_NO_SERVER)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

server: $(OBJ_DIR)/server.o
	$(CC) $^ -o $(SERVER) $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(SERVER)
