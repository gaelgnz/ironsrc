#ifndef SERVER_H
#define SERVER_H

#include "entity.h"
#include "protocol.h"
#include "stdint.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 16
#define MAX_ENTITIES 256
#define CHAT_LEN 1024

typedef struct Client {
    char username[16];
    int client_id;

    bool active;
    time_t last_seen;
    struct sockaddr_in sockaddr;
} Client;
typedef struct Server {
    Entity entities[MAX_ENTITIES];
    int entity_count;

    uint32_t tick;

    Client clients[MAX_CLIENTS];
    int client_count;
    pktUserUpdate last_client_updates[MAX_CLIENTS];

    char chat[CHAT_LEN];

    Map map;
} Server;

void sv_init(Server *server);
void sv_join_player(Server *server, const char username[16],
                    struct sockaddr_in client_sock_addr);
int sv_find_client(Server *sv, struct sockaddr_in addr);
void sv_tick(Server *server, float deltaTime);
void sv_spawn_entity(Server *server, Entity entity);
void sv_receive_update(Server *server, int client_id, pktUserUpdate cmd);
void sv_delete_player(Server *server, int client_id);
#endif
