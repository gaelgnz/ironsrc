/*
menu.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "menu.h"
#include "game.h"
#include "global.h"
#include "map_editor.h"
#include "raygui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

void menu_init(Global *global) {
    global->gamemode = GM_MENU;
    strcpy(global->menu.ip, "127.0.0.1");
    sprintf(global->menu.port, "%d", 4445);
    global->menu.map[0] = '\0';
}

void menu_loop(Global *global) {
    MenuState *state = &global->menu;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    BeginDrawing();
    ClearBackground(DARKGRAY);

    const char *title = "IRONSRC";
    int title_size = 40;
    DrawText(title, (sw - MeasureText(title, title_size)) / 2, 40, title_size,
             RAYWHITE);

    int btn_y = sh - 60;
    int btn_w = 120, btn_h = 34;
    int btn_spacing = 16;
    int total_w = btn_w * 3 + btn_spacing * 2;
    int btn_start_x = (sw - total_w) / 2;

    if (GuiButton((Rectangle){btn_start_x, btn_y, btn_w, btn_h}, "Host")) {
        state->host_menu = true;
        state->connect_menu = false;
    }
    if (GuiButton(
            (Rectangle){btn_start_x + btn_w + btn_spacing, btn_y, btn_w, btn_h},
            "Connect")) {
        state->connect_menu = true;
        state->host_menu = false;
    }
    if (GuiButton((Rectangle){btn_start_x + (btn_w + btn_spacing) * 2, btn_y,
                              btn_w, btn_h},
                  "Map Editor")) {
        memset(&global->editor, 0, sizeof(MapEditorState));
        map_editor_init(global);
        global->gamemode = GM_MAPEDITOR;
    }

    if (state->host_menu || state->connect_menu) {
        int panel_w = 320;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, Fade(BLACK, 0.55f));
        DrawRectangleLinesEx((Rectangle){panel_x, panel_y, panel_w, panel_h}, 2,
                             LIGHTGRAY);

        int tab_w = panel_w / 2;
        int tab_h = 32;
        bool host_tab = state->host_menu;
        bool connect_tab = state->connect_menu;
        if (GuiButton((Rectangle){panel_x, panel_y, tab_w, tab_h}, "Host")) {
            state->host_menu = true;
            state->connect_menu = false;
            state->map_edit_mode = false;
            state->ip_edit_mode = false;
            state->port_edit_mode = false;
        }

        if (GuiButton((Rectangle){panel_x + tab_w, panel_y, tab_w, tab_h},
                      "Connect")) {
            state->connect_menu = true;
            state->host_menu = false;
            state->map_edit_mode = false;
            state->ip_edit_mode = false;
            state->port_edit_mode = false;
        }

        int ul_x = host_tab ? panel_x : panel_x + tab_w;
        DrawRectangle(ul_x, panel_y + tab_h - 2, tab_w, 2, SKYBLUE);

        int content_y = panel_y + tab_h + 16;
        int field_x = panel_x + 20;
        int field_w = panel_w - 40;
        int label_h = 18;
        int field_h = 28;

        if (host_tab) {
            Rectangle map_box = {field_x, content_y + label_h, field_w,
                                 field_h};

            GuiLabel((Rectangle){field_x, content_y, 80, label_h}, "Map name:");
            GuiTextBox(map_box, state->map, 32, state->map_edit_mode);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state->map_edit_mode =
                    CheckCollisionPointRec(GetMousePosition(), map_box);
            }

            int action_y = panel_y + panel_h - 48;
            if (GuiButton((Rectangle){field_x, action_y, 120, 32},
                          "Start Host")) {
                EndDrawing();
                host(state->map);
                connect_sv(global, state->map);
                return;
            }
            if (GuiButton(
                    (Rectangle){panel_x + panel_w - 100, action_y, 80, 32},
                    "Close")) {
                state->host_menu = false;
                state->map_edit_mode = false;
            }
        }

        if (connect_tab) {
            Rectangle ip_box = {field_x, content_y + label_h, field_w, field_h};
            Rectangle port_box = {field_x,
                                  content_y + label_h * 2 + field_h + 10,
                                  field_w, field_h};

            GuiLabel((Rectangle){field_x, content_y, 80, label_h},
                     "IP address:");
            GuiLabel((Rectangle){field_x, content_y + label_h + field_h + 10,
                                 80, label_h},
                     "Port:");

            GuiTextBox(ip_box, state->ip, 64, state->ip_edit_mode);
            GuiTextBox(port_box, state->port, 16, state->port_edit_mode);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                bool on_ip = CheckCollisionPointRec(mouse, ip_box);
                bool on_port = CheckCollisionPointRec(mouse, port_box);
                state->ip_edit_mode = on_ip;
                state->port_edit_mode = on_port;
            }

            int action_y = panel_y + panel_h - 48;
            if (GuiButton((Rectangle){field_x, action_y, 120, 32}, "Connect")) {
                EndDrawing();
                connect_sv(global, state->map);
                return;
            }
            if (GuiButton(
                    (Rectangle){panel_x + panel_w - 100, action_y, 80, 32},
                    "Close")) {
                state->connect_menu = false;
                state->ip_edit_mode = false;
                state->port_edit_mode = false;
            }
        }
    }

    EndDrawing();
}
