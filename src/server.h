#ifndef SERVER_H
#define SERVER_H

#include "entity.h"
#include "protocol.h"
#include "stdint.h"

#define MAX_CLIENTS 16
#define MAX_ENTITIES 256

typedef struct Server {
    Entity entities[MAX_ENTITIES];
    int entity_count;
    uint32_t tick;
    UserCmd last_commands[MAX_CLIENTS];
} Server;

void sv_init(Server *server);
void sv_tick(Server *server, float deltaTime);
void sv_spawn_entity(Server *server, Entity entity);
void sv_receive_input(Server *server, int client_id, UserCmd cmd);

#endif
