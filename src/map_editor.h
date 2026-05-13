/*
map_editor.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef MAP_EDITOR_H
#define MAP_EDITOR_H

#include "map.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct MapEditorState {
    Camera2D camera;
    Map map;
    int selected;          // sector selection (-1 if none)
    int selected_entity;   // entity selection (-1 if none)
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
    bool npc_tex_edit_mode;
    char map_file_buf[64];
    char floor_buf[16];
    char ceil_buf[16];
    char npc_tex_buf[32];
    char npc_health_buf[16];
    char pos_x_buf[16];
    char pos_z_buf[16];
    int last_selected;
} MapEditorState;

typedef struct Global Global;
void map_editor_init(Global *global);
void map_editor_loop(Global *global);

#endif
