/*
map_editor.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "map_editor.h"
#include "assets.h"
#include "entity.h"
#include "global.h"
#include "map.h"
#include "math.h"
#include "menu.h"
#include "raygui.h"
#include "raylib.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 5
#define HANDLE_SIZE 8
#define MIN_SECTOR_SIZE 10

inline Vector2 snap_to_grid(Vector2 v) {
    return (Vector2){(int)(v.x / GRID_SIZE) * GRID_SIZE,
                     (int)(v.y / GRID_SIZE) * GRID_SIZE};
}

inline bool is_mouse_in_sector(Camera2D camera, Sector *sector) {
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    return mouse.x >= sector->x && mouse.x <= sector->x + sector->width &&
           mouse.y >= sector->y && mouse.y <= sector->y + sector->height;
}

static bool is_mouse_in_resize_handle(Camera2D camera, Sector *sector) {
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    float hx = sector->x + sector->width;
    float hy = sector->y + sector->height;
    return mouse.x >= hx - HANDLE_SIZE && mouse.x <= hx + HANDLE_SIZE &&
           mouse.y >= hy - HANDLE_SIZE && mouse.y <= hy + HANDLE_SIZE;
}
void save_map(Map *map, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(map, sizeof(Map), 1, f);
    fclose(f);
}
void map_editor_init(Global *global) {
    MapEditorState *s = &global->editor;
    s->camera = (Camera2D){
        .offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f},
        .target = (Vector2){0, 0},
        .rotation = 0,
        .zoom = 1.0f};
    s->map = *load_map("map.dat");
    s->selected = -1;
    s->dragging = false;
    s->resizing = false;
    s->creating = false;
    s->texture_edit_mode = false;
    s->floor_edit_mode = false;
    s->ceiling_edit_mode = false;
    memset(s->map_file_buf, 0, sizeof(s->map_file_buf));
    strncpy(s->map_file_buf, "map.dat", sizeof(s->map_file_buf));
    s->map_file_edit_mode = false;
}

void map_editor_loop(Global *global) {
    MapEditorState *s = &global->editor;
    float dt = GetFrameTime();
    Vector2 mouse = GetMousePosition();
    Vector2 world_mouse = GetScreenToWorld2D(mouse, s->camera);

    int panel_width = GetScreenWidth() / 3;
    bool mouse_in_panel =
        s->menu_open && mouse.x > GetScreenWidth() - panel_width;

    // Camera keyboard
    if (IsKeyDown(KEY_W))
        s->camera.target.y -= 200.f * dt;
    if (IsKeyDown(KEY_S))
        s->camera.target.y += 200.f * dt;
    if (IsKeyDown(KEY_A))
        s->camera.target.x -= 200.f * dt;
    if (IsKeyDown(KEY_D))
        s->camera.target.x += 200.f * dt;

    // Camera pan with middle mouse
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 delta = GetMouseDelta();
        s->camera.target.x -= delta.x / s->camera.zoom;
        s->camera.target.y -= delta.y / s->camera.zoom;
    }

    // Camera zoom
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && !mouse_in_panel) {
        s->camera.zoom += wheel * 0.1f;
        if (s->camera.zoom < 0.1f)
            s->camera.zoom = 0.1f;
    }

    if (IsKeyPressed(KEY_F1))
        s->menu_open = !s->menu_open;

    // Delete selected sector
    if (IsKeyPressed(KEY_X)) {

        for (int i = 0; i < s->map.entity_count; i++) {
            if (s->map.entities[i].type == ENT_PLAYER_START) {
                // Shift everything left
                for (int j = i; j < s->map.entity_count - 1; j++) {
                    s->map.entities[j] = s->map.entities[j + 1];
                }
                s->map.entity_count--;
                i--;
            }
        }

        // Add new one
        Entity e = (Entity){0};
        e.active = true;
        e.type = ENT_PLAYER_START;

        Vector2 pos = GetScreenToWorld2D(GetMousePosition(), s->camera);
        float floor_y = 0;
        for (int i = 0; i < s->map.sector_count; i++) {
            if (is_mouse_in_sector(s->camera, &s->map.sectors[i])) {
                floor_y = s->map.sectors[i].floor_height + 1;
                break;
            }
        }
        e.position = (Vector3){pos.x, floor_y, pos.y};

        s->map.entities[s->map.entity_count++] = e;
    }

    if (!mouse_in_panel) {
        // Start creating sector with right click drag
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            Vector2 snapped = snap_to_grid(world_mouse);
            s->create_start = snapped;
            s->creating = true;
            s->selected = -1;
        }

        if (IsKeyPressed(KEY_X)) {
            Entity e = (Entity){0};
            e.active = true;
            e.type = ENT_PLAYER_START;
            Vector2 pos = GetScreenToWorld2D(GetMousePosition(), s->camera);
            e.position = (Vector3){pos.x, 0, pos.y};
            s->map.entities[s->map.entity_count++] = e;
        }
        if (s->creating) {
            if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
                Vector2 snapped = snap_to_grid(world_mouse);
                int x = (int)fminf(s->create_start.x, snapped.x);
                int y = (int)fminf(s->create_start.y, snapped.y);
                int w = (int)fabsf(snapped.x - s->create_start.x);
                int h = (int)fabsf(snapped.y - s->create_start.y);
                if (w >= MIN_SECTOR_SIZE && h >= MIN_SECTOR_SIZE &&
                    s->map.sector_count < MAX_SECTORS) {
                    Sector ns = {0};
                    strncpy(ns.texture, "metal_01", 32);
                    ns.x = x;
                    ns.y = y;
                    ns.width = w;
                    ns.height = h;
                    ns.floor_height = 0;
                    ns.ceiling_height = 30;
                    ns.ceiling_enabled = true;
                    s->selected = s->map.sector_count;
                    s->map.sectors[s->map.sector_count++] = ns;
                }
                s->creating = false;
            }
        } else {
            // Resize
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && s->selected >= 0) {
                if (is_mouse_in_resize_handle(s->camera,
                                              &s->map.sectors[s->selected])) {
                    s->resizing = true;
                }
            }

            // Drag
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !s->resizing) {
                s->dragging = false;
                for (int i = s->map.sector_count - 1; i >= 0; i--) {
                    if (is_mouse_in_sector(s->camera, &s->map.sectors[i])) {
                        if (i == s->selected &&
                            !is_mouse_in_resize_handle(s->camera,
                                                       &s->map.sectors[i])) {
                            s->dragging = true;
                            s->drag_offset.x =
                                world_mouse.x - s->map.sectors[i].x;
                            s->drag_offset.y =
                                world_mouse.y - s->map.sectors[i].y;
                        }
                        s->selected = i;
                        break;
                    }
                }
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                s->dragging = false;
                s->resizing = false;
            }

            if (s->dragging && s->selected >= 0) {
                Sector *sec = &s->map.sectors[s->selected];
                Vector2 snapped =
                    snap_to_grid((Vector2){world_mouse.x - s->drag_offset.x,
                                           world_mouse.y - s->drag_offset.y});
                sec->x = (int)snapped.x;
                sec->y = (int)snapped.y;
            }

            if (s->resizing && s->selected >= 0) {
                Sector *sec = &s->map.sectors[s->selected];
                Vector2 snapped = snap_to_grid(world_mouse);
                int new_w = (int)snapped.x - sec->x;
                int new_h = (int)snapped.y - sec->y;
                if (new_w >= MIN_SECTOR_SIZE)
                    sec->width = new_w;
                if (new_h >= MIN_SECTOR_SIZE)
                    sec->height = new_h;
            }
        }
    }

    // Draw
    BeginDrawing();
    ClearBackground(BLUE);
    BeginMode2D(s->camera);
    Vector2 top_left = GetScreenToWorld2D((Vector2){0, 0}, s->camera);
    Vector2 bottom_right = GetScreenToWorld2D(
        (Vector2){GetScreenWidth(), GetScreenHeight()}, s->camera);

    int start_x = (int)(top_left.x / GRID_SIZE) * GRID_SIZE;
    int start_y = (int)(top_left.y / GRID_SIZE) * GRID_SIZE;

    for (int x = start_x; x < (int)bottom_right.x; x += GRID_SIZE)
        DrawLine(x, (int)top_left.y, x, (int)bottom_right.y,
                 (Color){255, 255, 255, 20});

    for (int y = start_y; y < (int)bottom_right.y; y += GRID_SIZE)
        DrawLine((int)top_left.x, y, (int)bottom_right.x, y,
                 (Color){255, 255, 255, 20});
    for (int i = 0; i < s->map.sector_count; i++) {
        Sector *sector = &s->map.sectors[i];
        unsigned char t = (i == s->selected) ? 255 : 100;
        Texture2D tex = get_texture(&global->assets, sector->texture);
        DrawTexturePro(
            tex, (Rectangle){0, 0, tex.width, tex.height},
            (Rectangle){sector->x, sector->y, sector->width, sector->height},
            (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, t});
        DrawRectangleLines(sector->x, sector->y, sector->width, sector->height,
                           (Color){255, 255, 255, t});
        if (i == s->selected) {
            DrawRectangle(sector->x + sector->width - HANDLE_SIZE,
                          sector->y + sector->height - HANDLE_SIZE,
                          HANDLE_SIZE * 2, HANDLE_SIZE * 2, YELLOW);
        }
    }

    for (int i = 0; i < s->map.entity_count; i++) {
        Entity *e = &s->map.entities[i];
        if (e->active && e->type == ENT_PLAYER_START) {
            DrawCircleV((Vector2){e->position.x, e->position.z}, 5, GREEN);
        }
    }

    // Preview new sector while creating
    if (s->creating) {
        Vector2 snapped = snap_to_grid(world_mouse);
        int x = (int)fminf(s->create_start.x, snapped.x);
        int y = (int)fminf(s->create_start.y, snapped.y);
        int w = (int)fabsf(snapped.x - s->create_start.x);
        int h = (int)fabsf(snapped.y - s->create_start.y);
        DrawRectangleLines(x, y, w, h, GREEN);
    }

    EndMode2D();
    if (s->menu_open) {

        Rectangle panel = {GetScreenWidth() - panel_width, 0, panel_width,
                           GetScreenHeight()};
        GuiWindowBox(panel, "Edit Sector");

        float px = panel.x + 10;
        float py = panel.y + 30;
        float fw = panel_width - 20;

        GuiLabel((Rectangle){px, py, fw, 20}, "Map file:");
        py += 20;
        Rectangle map_file_box = {px, py, fw - 60, 25};
        if (GuiTextBox(map_file_box, s->map_file_buf, sizeof(s->map_file_buf),
                       s->map_file_edit_mode))
            s->map_file_edit_mode = !s->map_file_edit_mode;
        if (GuiButton((Rectangle){px + fw - 55, py, 55, 25}, "Load")) {
            Map *loaded = load_map(s->map_file_buf);
            if (loaded) {
                s->map = *loaded;
                s->selected = -1;
            }
        }
        py += 35;

        if (s->selected >= 0 && s->selected < s->map.sector_count) {
            Sector *sec = &s->map.sectors[s->selected];
            if (s->selected != s->last_selected) {
                snprintf(s->floor_buf, sizeof(s->floor_buf), "%d",
                         sec->floor_height);
                snprintf(s->ceil_buf, sizeof(s->ceil_buf), "%d",
                         sec->ceiling_height);
                s->last_selected = s->selected;
            }
            GuiLabel((Rectangle){px, py, fw, 20},
                     TextFormat("Sector %d", s->selected));
            py += 25;

            GuiLabel((Rectangle){px, py, fw, 20}, "Texture:");
            py += 20;
            Rectangle tex_box = {px, py, fw, 25};
            if (GuiTextBox(tex_box, sec->texture, 32, s->texture_edit_mode))
                s->texture_edit_mode = !s->texture_edit_mode;
            py += 35;

            GuiLabel((Rectangle){px, py, fw, 20}, "Floor height:");
            py += 20;
            Rectangle floor_box = {px, py, fw, 25};

            if (GuiTextBox(floor_box, s->floor_buf, sizeof(s->floor_buf),
                           s->floor_edit_mode)) {
                s->floor_edit_mode = !s->floor_edit_mode;
                sec->floor_height = atoi(s->floor_buf);
            }

            py += 35;

            GuiLabel((Rectangle){px, py, fw, 20}, "Ceiling height:");
            py += 20;
            Rectangle ceil_box = {px, py, fw, 25};
            if (GuiTextBox(ceil_box, s->ceil_buf, sizeof(s->ceil_buf),
                           s->ceiling_edit_mode)) {
                s->ceiling_edit_mode = !s->ceiling_edit_mode;
                sec->ceiling_height = atoi(s->ceil_buf);
            }
            py += 35;

            GuiCheckBox((Rectangle){px, py, 20, 20}, "Ceiling enabled",
                        &sec->ceiling_enabled);
            py += 35;

            GuiLabel((Rectangle){px, py, fw, 20},
                     TextFormat("Pos: %d, %d", sec->x, sec->y));
            py += 20;
            GuiLabel((Rectangle){px, py, fw, 20},
                     TextFormat("Size: %d x %d", sec->width, sec->height));
            py += 35;

            if (GuiButton((Rectangle){px, py, fw, 25}, "Save"))
                save_map(&s->map, s->map_file_buf);
        } else {
            GuiLabel((Rectangle){px, py, fw, 20}, "No sector selected");
            py += 25;
            GuiLabel((Rectangle){px, py, fw, 20}, "Right-drag to create");
        }
    }
    DrawFPS(0, 0);
    EndDrawing();
}
