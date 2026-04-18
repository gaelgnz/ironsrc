CC = clang
CXX = clang++

RLIMGUI_PATH = rlImGui
IMGUI_PATH = cimgui/imgui

CFLAGS = -Wall -Wextra -Iraylib/src -Isrc -I$(RLIMGUI_PATH) -I$(IMGUI_PATH)
CXXFLAGS = -Iraylib/src -Isrc -I$(RLIMGUI_PATH) -I$(IMGUI_PATH)

LDFLAGS = -Lraylib/src -lraylib -lm -lpthread -ldl -lrt -lX11 -lglfw

BUILD_DIR = build
SRC_DIR = src

SRCS = $(filter-out $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

IMGUI_OBJS = \
    $(BUILD_DIR)/imgui.o \
    $(BUILD_DIR)/imgui_draw.o \
    $(BUILD_DIR)/imgui_tables.o \
    $(BUILD_DIR)/imgui_widgets.o \
    $(BUILD_DIR)/rlImGui.o

.PHONY: all server clean

all: ironsrc

ironsrc: $(OBJS) $(IMGUI_OBJS)
	$(CXX) $(OBJS) $(IMGUI_OBJS) -o ironsrc $(LDFLAGS)

server: $(BUILD_DIR)/server.o
	$(CC) $(BUILD_DIR)/server.o -o server $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/imgui.o: $(IMGUI_PATH)/imgui.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/imgui_draw.o: $(IMGUI_PATH)/imgui_draw.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/imgui_tables.o: $(IMGUI_PATH)/imgui_tables.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/imgui_widgets.o: $(IMGUI_PATH)/imgui_widgets.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/rlImGui.o: $(RLIMGUI_PATH)/rlImGui.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ironsrc server
