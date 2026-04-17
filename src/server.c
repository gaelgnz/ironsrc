#include "server.h"
#include "entity.h"
#include "protocol.h"
#include "raymath.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define PORT 4445
#define MAX_BUF 1024
#define TIMEOUT_SEC 5.0

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

void sv_broadcast(Server *server, int listenfd) {
    static uint8_t buf[sizeof(Packet) + sizeof(pktServerUpdate)];
    Packet *pkt = (Packet *)buf;
    pkt->type = PKT_SERVER_UPDATE;
    pktServerUpdate *upd = (pktServerUpdate *)pkt->data;
    upd->tick = server->tick;

    for (int c = 0; c < server->client_count; c++) {
        printf("broadcasting %d entities to client %d\n", upd->entity_count, c);
        upd->entity_count = 0;

        for (int i = 0; i < server->entity_count; i++) {
            Entity *e = &server->entities[i];
            if (!e->active)
                continue;

            // skip sending a player their own entity
            if (e->type == ENT_PLAYER && e->client_id == c)
                continue;

            NetEntity ne = {0};
            ne.type = e->type;
            ne.position = e->position;
            ne.velocity = e->velocity;
            ne.active = e->active;

            // copy the relevant union member
            switch (e->type) {
            case ENT_PLAYER:
                ne.player = e->player;
                break;
            case ENT_NPC_GENERIC:
                ne.npc = e->npc;
                break;
            case ENT_PROP:
                ne.prop = e->prop;
                break;
            default:
                break;
            }

            upd->entities[upd->entity_count++] = ne;
        }

        sendto(listenfd, buf, sizeof(buf), 0,
               (struct sockaddr *)&server->clients[c].sockaddr,
               sizeof(server->clients[c].sockaddr));
    }
}
int sv_find_client(Server *sv, struct sockaddr_in addr) {
    for (int i = 0; i < sv->client_count; i++) {
        if (sv->clients[i].sockaddr.sin_addr.s_addr == addr.sin_addr.s_addr &&
            sv->clients[i].sockaddr.sin_port == addr.sin_port) {
            return i;
        }
    }
    return -1;
}
static double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
void sv_join_player(Server *server, const char username[],
                    struct sockaddr_in addr) {

    int id = server->client_count;

    Client *c = &server->clients[id];
    memset(c, 0, sizeof(Client));

    strncpy(c->username, username, sizeof(c->username) - 1);
    c->client_id = id;
    c->sockaddr = addr;
    c->last_seen = get_time();

    Entity *e = &server->entities[server->entity_count];
    memset(e, 0, sizeof(Entity));

    e->type = ENT_PLAYER;
    e->client_id = id;
    e->position = (Vector3){0, 0, 0};
    e->active = true;

    server->client_count++;
    server->entity_count++;

    printf("%s joined (%d players)\n", username, server->client_count);
}

void sv_delete_player(Server *server, int client_id) {
    if (client_id < 0 || client_id >= server->client_count)
        return;

    Client *c = &server->clients[client_id];

    printf("%s left (%d players)\n", c->username, server->client_count - 1);

    for (int i = 0; i < server->entity_count; i++) {
        if (server->entities[i].client_id == client_id) {
            server->entities[i] = server->entities[server->entity_count - 1];
            server->entity_count--;
            break;
        }
    }

    server->clients[client_id] = server->clients[server->client_count - 1];

    for (int i = 0; i < server->entity_count; i++) {
        if (server->entities[i].client_id == server->client_count - 1) {
            server->entities[i].client_id = client_id;
        }
    }

    server->client_count--;
}

void sv_receive_update(Server *server, int client_id, pktUserUpdate cmd) {

    server->last_client_updates[client_id] = cmd;
}

void sv_init(Server *server) {
    memset(server, 0, sizeof(Server));
    Entity npc = {0};
    npc.client_id = NOT_PLAYER;
    npc.position = Vector3One();
    npc.type = ENT_NPC_GENERIC;
    npc.active = true;
    printf("npc.active before assign: %d, true=%d\n", npc.active, (int)true);
    server->entities[0] = npc;
    printf("after assign: %d\n", server->entities[0].active);
    server->entity_count = 1;
}

void sv_tick(Server *server, float dt) {
    for (int i = 0; i < server->entity_count; i++) {
        Entity *e = &server->entities[i];
        if (!e->active)
            continue;

        if (e->client_id != NOT_PLAYER) {
            pktUserUpdate upd = server->last_client_updates[e->client_id];

            e->position = upd.position;
            e->velocity = upd.current_velocity;
        }
        e->velocity.y -= 20.0f * dt;
        e->velocity.x *= 0.8f;
        e->velocity.z *= 0.8f;

        if (e->position.y < 0.0f) {
            e->position.y = 0.0f;
            e->velocity.y = 0.0f;
        }

        e->position = Vector3Add(e->position, Vector3Scale(e->velocity, dt));
    }
    for (int i = 0; i < server->entity_count; i++) {
        Entity *e = &server->entities[i];
        printf("  entity %d: active=%d type=%d client_id=%d\n", i, e->active,
               e->type, e->client_id);
        if (!e->active)
            continue;
        // ...
    }
    server->tick++;
    printf("sv_tick entity_count before broadcast: %d\n", server->entity_count);
    printf("sizeof pktServerUpdate: %zu\n", sizeof(pktServerUpdate));
    printf("sizeof buf: %zu\n", sizeof(Packet) + sizeof(pktServerUpdate));
}

typedef struct RecvThreadArgs {
    int listenfd;
    Server *sv;
} RecvThreadArgs;

void *recv_thread(void *arg) {
    RecvThreadArgs *args = (RecvThreadArgs *)arg;
    Server *sv = args->sv;
    int listenfd = args->listenfd;

    free(args);

    struct sockaddr_in client_addr;
    socklen_t len;

    struct timeval tv = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (;;) {
        uint8_t buf[MAX_BUF];
        len = sizeof(client_addr);

        int n = recvfrom(listenfd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client_addr, &len);

        pthread_mutex_lock(&server_mutex);

        if (n < 0) {
            double t = get_time();

            for (int i = 0; i < sv->client_count; i++) {
                printf("client %d timed out (last_seen: %f, now: %f)\n", i,
                       sv->clients[i].last_seen, t);
                if (t - sv->clients[i].last_seen > TIMEOUT_SEC) {
                    sv_delete_player(sv, i);
                    i--;
                }
            }

            pthread_mutex_unlock(&server_mutex);
            continue;
        }

        Packet *pkt = (Packet *)buf;

        int id = sv_find_client(sv, client_addr);

        switch (pkt->type) {

        case PKT_USER_JOIN: {
            pktUserJoin *join = (pktUserJoin *)pkt->data;

            sv_join_player(sv, join->userName, client_addr);

            int new_id = sv->client_count - 1;
            sv->clients[new_id].last_seen = get_time();

            uint8_t ack_buf[sizeof(Packet) + sizeof(pktUserJoinAck)];
            Packet *ack = (Packet *)ack_buf;
            ack->type = PKT_USER_JOIN_ACK;

            pktUserJoinAck *ackd = (pktUserJoinAck *)ack->data;
            ackd->accepted = true;

            sendto(listenfd, ack_buf, sizeof(ack_buf), 0,
                   (struct sockaddr *)&client_addr, len);
            break;
        }

        case PKT_USER_UPDATE: {
            if (id == -1)
                break;

            sv->clients[id].last_seen = get_time();

            pktUserUpdate *upd = (pktUserUpdate *)pkt->data;
            sv_receive_update(sv, id, *upd);
            break;
        }

        case PKT_USER_DISCONNECT: {
            printf("received disconnect\n");
            if (id != -1)
                sv_delete_player(sv, id);
            break;
        }
        }

        pthread_mutex_unlock(&server_mutex);
    }

    return NULL;
}

int main() {
    int listenfd;
    struct sockaddr_in addr;

    listenfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));

    printf("listening on %d\n", PORT);

    Server sv = {0};
    sv_init(&sv);

    RecvThreadArgs *args = malloc(sizeof(RecvThreadArgs));
    args->listenfd = listenfd;
    args->sv = &sv;

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, args);

    struct timespec last, now;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        double dt =
            (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;

        if (dt >= (1.0 / 20.0)) {
            pthread_mutex_lock(&server_mutex);
            sv_tick(&sv, dt);
            printf("after sv_init: active=%d\n", sv.entities[0].active);
            sv_broadcast(&sv, listenfd);
            pthread_mutex_unlock(&server_mutex);

            last = now;
        }
    }

    pthread_join(tid, NULL);
    return 0;
}
