#ifndef GLOBAL_H
#define GLOBAL_H

#include "assets.h"
#include "game.h"
#include "map.h"
#include "menu.h"
#include "protocol.h"
#include "raylib.h"
#include <arpa/inet.h>
#include <netinet/in.h>

typedef enum GameMode {
    GM_MENU,
    GM_INGAME,
} GameMode;

typedef struct Global {
    GameMode gamemode;
    Assets assets;
    union {
        IngameState ingame;
        MenuState menu;
    };
} Global;

#endif
