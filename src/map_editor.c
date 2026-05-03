#include "map_editor.h"
#include "assets.h"
#include "global.h"
#include "map.h"
#include "raygui.h"
#include "raylib.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 10
#define HANDLE_SIZE 8

static Rectangle sector_rect(Sector *s, Vector2 cam) {
    return (Rectangle){s->x + cam.x, s->y + cam.y, s->width, s->height};
}

static void draw_sector(Sector *s, int selected, Vector2 cam) {
    Rectangle r = sector_rect(s, cam);
    Color col = selected ? GREEN : GRAY;
    DrawRectangleRec(r, Fade(col, 0.5f));
    DrawRectangleLinesEx(r, 2, col);

    if (selected) {
        DrawRectangle(r.x, r.y, HANDLE_SIZE, HANDLE_SIZE, RED);
        DrawRectangle(r.x + r.width - HANDLE_SIZE, r.y, HANDLE_SIZE,
                      HANDLE_SIZE, RED);
        DrawRectangle(r.x, r.y + r.height - HANDLE_SIZE, HANDLE_SIZE,
                      HANDLE_SIZE, RED);
        DrawRectangle(r.x + r.width - HANDLE_SIZE,
                      r.y + r.height - HANDLE_SIZE, HANDLE_SIZE, HANDLE_SIZE,
                      RED);
    }

    char label[64];
    sprintf(label, "%s (fl:%d cl:%d)", s->texture, s->floor_height,
            s->ceiling_height);
    DrawText(label, r.x + 5, r.y + 5, 10, WHITE);
}

static int find_sector_at(Map *map, Vector2 pos, Vector2 cam) {
    for (int i = map->sector_count - 1; i >= 0; i--) {
        Rectangle r = sector_rect(&map->sectors[i], cam);
        if (CheckCollisionPointRec(pos, r))
            return i;
    }
    return -1;
}

static int snap(int v) { return (v / GRID_SIZE) * GRID_SIZE; }

void map_editor_loop(Global *global) {
    MapEditorState *state = &global->editor;

    // Initialize if first time
    if (!state->initialized) {
        memset(state, 0, sizeof(MapEditorState));
        state->map = (Map){0};
        state->selected_sector = -1;
        strcpy(state->filename, "map.dat");
        state->initialized = 1;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawText("Map Editor (LMB: draw/add, RMB: select, Drag handles, F1: controls)",
             20, 20, 20, WHITE);
    DrawText(state->status, 20, 50, 18, YELLOW);

    if (IsKeyPressed(KEY_F1))
        state->show_controls = !state->show_controls;

    Vector2 cam = state->camera;
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 delta = GetMouseDelta();
        cam.x += delta.x;
        cam.y += delta.y;
    }
    if (IsKeyDown(KEY_LEFT))
        cam.x += 5;
    if (IsKeyDown(KEY_RIGHT))
        cam.x -= 5;
    if (IsKeyDown(KEY_UP))
        cam.y += 5;
    if (IsKeyDown(KEY_DOWN))
        cam.y -= 5;
    state->camera = cam;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Full window editor area
    Rectangle editor = {0, 0, sw, sh};

    // Draw grid
    for (int x = ((int)cam.x % GRID_SIZE); x < sw; x += GRID_SIZE)
        DrawLine(x, 0, x, sh, Fade(GRAY, 0.3f));
    for (int y = ((int)cam.y % GRID_SIZE); y < sh; y += GRID_SIZE)
        DrawLine(0, y, sw, y, Fade(GRAY, 0.3f));

    // Draw sectors
    for (int i = 0; i < state->map.sector_count; i++) {
        draw_sector(&state->map.sectors[i], i == state->selected_sector, cam);
    }

    // Draw entities
    for (int i = 0; i < state->map.entity_count; i++) {
        Entity *e = &state->map.entities[i];
        if (!e->active) continue;
        Vector2 ent_pos = {e->position.x + cam.x, e->position.z + cam.y};
        Color col = e->type == ENT_PLAYER_START ? GREEN : YELLOW;
        DrawCircle(ent_pos.x, ent_pos.y, 5, col);
        DrawText(TextFormat("%d", i), ent_pos.x + 8, ent_pos.y - 4, 10, WHITE);
    }

    Vector2 mouse = GetMousePosition();
    Vector2 world_mouse = {mouse.x - cam.x, mouse.y - cam.y};
    int hovered = find_sector_at(&state->map, mouse, cam);

    if (CheckCollisionPointRec(mouse, editor)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state->selected_sector >= 0 &&
                state->selected_sector < state->map.sector_count) {
                Sector *s = &state->map.sectors[state->selected_sector];
                Rectangle r = sector_rect(s, cam);
                Rectangle handles[] = {
                    {r.x, r.y, HANDLE_SIZE, HANDLE_SIZE},
                    {r.x + r.width - HANDLE_SIZE, r.y, HANDLE_SIZE,
                     HANDLE_SIZE},
                    {r.x, r.y + r.height - HANDLE_SIZE, HANDLE_SIZE,
                     HANDLE_SIZE},
                    {r.x + r.width - HANDLE_SIZE,
                     r.y + r.height - HANDLE_SIZE, HANDLE_SIZE,
                     HANDLE_SIZE}};
                for (int i = 0; i < 4; i++) {
                    if (CheckCollisionPointRec(mouse, handles[i])) {
                        state->mode = EDITOR_RESIZING;
                        state->resize_handle = i;
                        state->drag_start = world_mouse;
                        // Save original values
                        state->orig_x = s->x;
                        state->orig_y = s->y;
                        state->orig_width = s->width;
                        state->orig_height = s->height;
                        goto done;
                    }
                }
            }
            if (hovered >= 0) {
                state->selected_sector = hovered;
            } else {
                state->mode = EDITOR_DRAWING;
                state->drag_start = world_mouse;
                state->drag_end = world_mouse;
            }
        }
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (hovered >= 0)
                state->selected_sector = hovered;
        }
    }

done:
    if (state->mode == EDITOR_DRAWING && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        state->drag_end = world_mouse;
        int x = snap(state->drag_start.x);
        int y = snap(state->drag_start.y);
        int w = snap(state->drag_end.x - state->drag_start.x);
        int h = snap(state->drag_end.y - state->drag_start.y);
        DrawRectangleLinesEx(
            (Rectangle){x + cam.x, y + cam.y, w, h},
            2, YELLOW);
    }
    if (state->mode == EDITOR_DRAWING &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        int x = snap(state->drag_start.x);
        int y = snap(state->drag_start.y);
        int w = snap(state->drag_end.x - state->drag_start.x);
        int h = snap(state->drag_end.y - state->drag_start.y);
        if (w < 0) {
            x += w;
            w = -w;
        }
        if (h < 0) {
            y += h;
            h = -h;
        }
        if (w >= GRID_SIZE && h >= GRID_SIZE &&
            state->map.sector_count < MAX_SECTORS) {
            Sector *s = &state->map.sectors[state->map.sector_count];
            s->x = x;
            s->y = y;
            s->width = w;
            s->height = h;
            s->floor_height = 10;
            s->ceiling_height = 20;
            s->ceiling_enabled = true;
            strcpy(s->texture, "dirt_01");
            state->selected_sector = state->map.sector_count;
            state->map.sector_count++;
            strcpy(state->status, "Sector added!");
        }
        state->mode = EDITOR_NONE;
    }

    if (state->mode == EDITOR_RESIZING &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state->mode = EDITOR_NONE;
    }

    if (state->mode == EDITOR_RESIZING) {
        Sector *s = &state->map.sectors[state->selected_sector];
        int dx = snap(world_mouse.x - state->drag_start.x);
        int dy = snap(world_mouse.y - state->drag_start.y);

        // Calculate new values from original + delta
        int new_x = state->orig_x;
        int new_y = state->orig_y;
        int new_w = state->orig_width;
        int new_h = state->orig_height;

        switch (state->resize_handle) {
        case 0: // top-left
            new_x = state->orig_x + dx;
            new_y = state->orig_y + dy;
            new_w = state->orig_width - dx;
            new_h = state->orig_height - dy;
            break;
        case 1: // top-right
            new_y = state->orig_y + dy;
            new_w = state->orig_width + dx;
            new_h = state->orig_height - dy;
            break;
        case 2: // bottom-left
            new_x = state->orig_x + dx;
            new_w = state->orig_width - dx;
            new_h = state->orig_height + dy;
            break;
        case 3: // bottom-right
            new_w = state->orig_width + dx;
            new_h = state->orig_height + dy;
            break;
        }

        // Apply snap and minimum size
        s->x = snap(new_x);
        s->y = snap(new_y);
        s->width = snap(new_w);
        s->height = snap(new_h);

        if (s->width < GRID_SIZE)
            s->width = GRID_SIZE;
        if (s->height < GRID_SIZE)
            s->height = GRID_SIZE;
    }

    // Floating controls - top right
    if (state->show_controls) {
        int cx = sw - 270;
        int cy = 20;
        DrawRectangle(cx, cy, 250, 500, (Color){50, 50, 50, 230});

        DrawText("Selected Sector:", cx + 10, cy + 10, 18, WHITE);
        cy += 35;

        if (state->selected_sector >= 0 &&
            state->selected_sector < state->map.sector_count) {
            Sector *s = &state->map.sectors[state->selected_sector];
            DrawText(TextFormat("Pos: %d, %d", s->x, s->y), cx + 10, cy, 16,
                     WHITE);
            cy += 20;
            DrawText(TextFormat("Size: %d x %d", s->width, s->height), cx + 10,
                     cy, 16, WHITE);
            cy += 25;

            // Texture
            DrawText("Texture:", cx + 10, cy, 16, WHITE);
            cy += 20;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                bool clicked = CheckCollisionPointRec(
                    GetMousePosition(), (Rectangle){cx + 10, cy, 180, 25});
                state->texture_edit_mode = clicked;
                if (clicked) {
                    state->floor_edit_mode = false;
                    state->ceil_edit_mode = false;
                }
            }
            GuiTextBox((Rectangle){cx + 10, cy, 180, 25}, s->texture, 32,
                       state->texture_edit_mode);
            cy += 30;

            // Floor H
            DrawText("Floor H:", cx + 10, cy, 16, WHITE);
            if (!state->floor_edit_mode) {
                sprintf(state->floor_h_str, "%d", s->floor_height);
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                bool clicked = CheckCollisionPointRec(
                    GetMousePosition(), (Rectangle){cx + 70, cy, 60, 25});
                state->floor_edit_mode = clicked;
                if (clicked) {
                    state->texture_edit_mode = false;
                    state->ceil_edit_mode = false;
                }
            }
            if (GuiTextBox((Rectangle){cx + 70, cy, 60, 25}, state->floor_h_str, 8,
                           state->floor_edit_mode)) {
                s->floor_height = atoi(state->floor_h_str);
            }
            cy += 30;

            // Ceil H
            DrawText("Ceil H:", cx + 10, cy, 16, WHITE);
            if (!state->ceil_edit_mode) {
                sprintf(state->ceil_h_str, "%d", s->ceiling_height);
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                bool clicked = CheckCollisionPointRec(
                    GetMousePosition(), (Rectangle){cx + 70, cy, 60, 25});
                state->ceil_edit_mode = clicked;
                if (clicked) {
                    state->texture_edit_mode = false;
                    state->floor_edit_mode = false;
                }
            }
            if (GuiTextBox((Rectangle){cx + 70, cy, 60, 25}, state->ceil_h_str, 8,
                           state->ceil_edit_mode)) {
                s->ceiling_height = atoi(state->ceil_h_str);
            }
            cy += 30;

            // Ceil On
            DrawText("Ceil On:", cx + 10, cy, 16, WHITE);
            GuiCheckBox((Rectangle){cx + 70, cy, 25, 25}, NULL,
                       &s->ceiling_enabled);
            cy += 35;

            if (GuiButton((Rectangle){cx + 10, cy, 100, 30}, "Delete")) {
                for (int i = state->selected_sector;
                     i < state->map.sector_count - 1; i++) {
                    state->map.sectors[i] = state->map.sectors[i + 1];
                }
                state->map.sector_count--;
                state->selected_sector = -1;
            }
            cy += 40;
        }

        // Add entity button
        if (GuiButton((Rectangle){cx + 10, cy, 120, 30}, "Add PLAYER_START")) {
            if (state->map.entity_count < MAX_ENTITIES) {
                Entity *e = &state->map.entities[state->map.entity_count];
                memset(e, 0, sizeof(Entity));
                e->type = ENT_PLAYER_START;
                e->active = true;
                e->id = state->map.entity_count;
                e->position = (Vector3){sw/2 - cam.x, 0, sh/2 - cam.y};
                state->map.entity_count++;
                strcpy(state->status, "Added PLAYER_START");
            }
        }
        cy += 40;

        // File operations
        DrawText("File:", cx + 10, cy, 18, WHITE);
        cy += 25;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool clicked = CheckCollisionPointRec(
                GetMousePosition(), (Rectangle){cx + 10, cy, 180, 25});
            state->file_edit_mode = clicked;
        }
        GuiTextBox((Rectangle){cx + 10, cy, 180, 25}, state->filename, 64,
                   state->file_edit_mode);
        cy += 35;

        if (GuiButton((Rectangle){cx + 10, cy, 100, 30}, "Save")) {
            FILE *f = fopen(state->filename, "wb");
            if (f) {
                fwrite(&state->map, sizeof(Map), 1, f);
                fclose(f);
                strcpy(state->status, "Map saved!");
            } else {
                sprintf(state->status, "Failed: %s", state->filename);
            }
        }
        cy += 40;
        if (GuiButton((Rectangle){cx + 10, cy, 100, 30}, "Load")) {
            FILE *f = fopen(state->filename, "rb");
            if (f) {
                fread(&state->map, sizeof(Map), 1, f);
                fclose(f);
                strcpy(state->status, "Map loaded!");
            } else {
                sprintf(state->status, "Failed: %s", state->filename);
            }
        }
    }

    DrawText("F1: toggle controls", 20, sh - 80, 16, GRAY);
    DrawText("MMB/Arrows: move camera", 20, sh - 60, 16, GRAY);

    if (GuiButton((Rectangle){20, sh - 40, 120, 30}, "Back to Menu")) {
        global->gamemode = GM_MENU;
    }

    EndDrawing();
}
