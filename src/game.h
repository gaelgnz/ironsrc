#ifndef GAME_H
#define GAME_H

#include "protocol.h"
#include "raylib.h"

#include <arpa/inet.h>
#include <netinet/in.h>

typedef struct IngameState {
    Vector3 position, velocity;
    float yaw, pitch;
    int sockfd;
    struct sockaddr_in sv_addr;

    // server state
    NetEntity entities[256];
    int entity_count;
    pthread_mutex_t entity_mutex;

    Font default_font; // this is temporary, fonts will be added to assets.c
    Entity myself;     // entity from server containing the local player

    char chat[8192];

    Map *map;
} IngameState;

typedef struct Global Global;
void game_loop(Global *global);
void connect_sv(Global *global);
void host();
#endif
