#ifndef MAP_EDITOR_H
#define MAP_EDITOR_H

#include "map.h"
#include "raylib.h"

typedef enum {
    EDITOR_NONE,
    EDITOR_DRAWING,
    EDITOR_RESIZING,
    EDITOR_DRAGGING
} EditorMode;

typedef struct MapEditorState {
    Map map;
    int selected_sector;
    char filename[64];
    char status[128];

    // 2D editor state
    EditorMode mode;
    Vector2 drag_start;
    Vector2 drag_end;
    int resize_handle;

    Vector2 camera;
    int show_controls;

    // Resize state
    int orig_x, orig_y, orig_width, orig_height;

    // Text edit modes
    int texture_edit_mode;
    int ceil_edit_mode;
    int floor_edit_mode;
    int file_edit_mode;

    // Persisted strings for text boxes
    char floor_h_str[8];
    char ceil_h_str[8];
} MapEditorState;

typedef struct Global Global;
void map_editor_loop(Global *global);

#endif
