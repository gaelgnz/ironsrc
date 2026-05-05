/*
global.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef GLOBAL_H
#define GLOBAL_H

#include "assets.h"
#include "game.h"
#include "map.h"
#include "map_editor.h"
#include "menu.h"
#include "protocol.h"
#include "raylib.h"
#include <arpa/inet.h>
#include <netinet/in.h>

typedef enum GameMode {
    GM_MENU,
    GM_INGAME,
    GM_MAPEDITOR,
} GameMode;

typedef struct Global {
    GameMode gamemode;
    Assets assets;
    union {
        IngameState ingame;
        MenuState menu;
        MapEditorState editor;
    };
} Global;

#endif
