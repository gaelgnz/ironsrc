#ifndef SERVER_H
#define SERVER_H

#include "entity.h"
#include "protocol.h"
#include "stdint.h"

#include <netinet/in.h>

#define MAX_CLIENTS 16
#define MAX_ENTITIES 256

typedef struct Client {
    char username[16];
    int client_id;
    struct sockaddr_in sockaddr;
} Client;
typedef struct Server {
    Entity entities[MAX_ENTITIES];
    int entity_count;

    uint32_t tick;

    Client clients[5];
    int client_count;
    pktUserUpdate last_client_updates[MAX_CLIENTS];
} Server;

void sv_init(Server *server);
void sv_join_player(Server *server, const char username[16],
                    struct sockaddr_in client_sock_addr);
void sv_tick(Server *server, float deltaTime);
void sv_spawn_entity(Server *server, Entity entity);
void sv_receive_update(Server *server, int client_id, pktUserUpdate cmd);
void sv_delete_player(Server *server, int client_id);
#endif
