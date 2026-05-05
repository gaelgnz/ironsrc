#include "assets.h"
#include "game.h"
#include "global.h"
#include "map_editor.h"
#include "menu.h"
#include "raygui.h"
#include "raylib.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    InitWindow(800, 600, "IronSrc");
    SetTargetFPS(60);

    SetExitKey(-1);

    Global global = {0};
    global.gamemode = GM_MENU;
    strcpy(global.menu.ip, "127.0.0.1");
    sprintf(global.menu.port, "%d", 4445);
    global.assets = assets_load();

    global.assets.default_font = LoadFont(
        "assets/fonts/font.ttf"); // dosent like being called in assets_load
    while (!WindowShouldClose()) {

        switch (global.gamemode) {
        case GM_MENU:
            menu_loop(&global);
            break;
        case GM_INGAME:
            game_loop(&global);
            break;
        case GM_MAPEDITOR:
            map_editor_loop(&global);
            break;
        }
    }

    CloseWindow();
    return 0;
}
