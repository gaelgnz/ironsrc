#ifndef MAP_EDITOR_H
#define MAP_EDITOR_H

#include "map.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct MapEditorState {
    Camera2D camera;
    Map map;
    int selected;
    bool menu_open;
    bool dragging;
    bool resizing;
    bool creating;
    Vector2 drag_offset;
    Vector2 create_start;
    bool texture_edit_mode;
    bool floor_edit_mode;
    bool ceiling_edit_mode;
    bool map_file_edit_mode;
    char map_file_buf[64];
    char floor_buf[16];
    char ceil_buf[16];
    int last_selected;
} MapEditorState;

typedef struct Global Global;
void map_editor_init(Global *global);
void map_editor_loop(Global *global);

#endif
