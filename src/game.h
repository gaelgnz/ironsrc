/*
game.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef GAME_H
#define GAME_H

#include "protocol.h"
#include "raylib.h"

#include <arpa/inet.h>
#include <netinet/in.h>

typedef enum InputState {
    IS_CHAT,
    IS_MOVING,
    IS_MENU,
} InputState;

typedef struct IngameState {
    InputState input_state;
    char message[32];
    int message_len;

    Vector3 position, velocity;
    float yaw, pitch;
    int sockfd;
    struct sockaddr_in sv_addr;

    // server state
    NetEntity entities[256];
    int entity_count;
    pthread_mutex_t entity_mutex;

    Font default_font;
    Entity myself;

    char chat[2048];

    Map *map;

    // audio
    Sound chat_sound;
    int chat_sound_loaded;

    // crouch state
    int crouching;
} IngameState;

typedef struct Global Global;
void game_loop(Global *global);
void connect_sv(Global *global);
void host();
#endif
