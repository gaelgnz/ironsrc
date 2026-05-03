#include "menu.h"
#include "game.h"
#include "global.h"
#include "raygui.h"
#include "raylib.h"
#include <stdio.h>
void menu_loop(Global *global) {
    MenuState *state = &global->menu;

    BeginDrawing();
    ClearBackground(GRAY);
    // GuiSetFont(global->assets.default_font);
    if (GuiButton((Rectangle){24, GetScreenHeight() - 60, 120, 30}, "Host")) {
        state->host_menu = true;
    }

    if (state->host_menu) {
        Rectangle host_win = {50, 50, 300, 300};
        GuiWindowBox(host_win, "Host");

        if (GuiButton((Rectangle){host_win.x + 20, host_win.y + 40, 120, 30},
                      "Start Host")) {
            host();
            connect_sv(global);
        }

        if (GuiButton((Rectangle){host_win.x + 20, host_win.y + 80, 120, 30},
                      "Close")) {
            state->host_menu = false;
        }
    }

    if (state->connect_menu) {
        Rectangle connect_win = {380, 50, 300, 300};
        GuiWindowBox(connect_win, "Connect");

        Rectangle ip_box = {connect_win.x + 20, connect_win.y + 40, 200, 30};
        Rectangle port_box = {connect_win.x + 20, connect_win.y + 90, 200, 30};

        GuiLabel((Rectangle){ip_box.x, ip_box.y - 20, 100, 20}, "IP:");
        GuiLabel((Rectangle){port_box.x, port_box.y - 20, 100, 20}, "Port:");

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool clicked_ip = CheckCollisionPointRec(GetMousePosition(), ip_box);
            bool clicked_port = CheckCollisionPointRec(GetMousePosition(), port_box);
            if (clicked_ip) {
                state->ip_edit_mode = true;
                state->port_edit_mode = false;
            } else if (clicked_port) {
                state->port_edit_mode = true;
                state->ip_edit_mode = false;
            } else {
                state->ip_edit_mode = false;
                state->port_edit_mode = false;
            }
        }

        GuiTextBox(ip_box, state->ip, 64, state->ip_edit_mode);
        GuiTextBox(port_box, state->port, 16, state->port_edit_mode);

        if (GuiButton(
                (Rectangle){connect_win.x + 20, connect_win.y + 140, 120, 30},
                "Connect")) {
            connect_sv(global);
        }

        if (GuiButton(
                (Rectangle){connect_win.x + 160, connect_win.y + 140, 120, 30},
                "Close")) {
            state->connect_menu = false;
        }
    }

    if (GuiButton((Rectangle){160, GetScreenHeight() - 60, 120, 30},
                  "Connect")) {
        state->connect_menu = true;
    }

    if (GuiButton((Rectangle){296, GetScreenHeight() - 60, 120, 30},
                  "Map Editor")) {
        global->gamemode = GM_MAPEDITOR;
    }

    EndDrawing();
}
