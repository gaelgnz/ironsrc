/*
server.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "server.h"
#include "entity.h"
#include "map.h"
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
#define STEP_HEIGHT 1.0f

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

Entity *entity_from_client_id(Server *sv, int client_id) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (sv->entities[i].client_id == client_id) {
            return &sv->entities[i];
        }
    }
    return NULL;
}
void sv_broadcast(Server *server, int listenfd) {
    static uint8_t buf[sizeof(Packet) + sizeof(pktServerUpdate)];
    Packet *pkt = (Packet *)buf;
    pkt->type = PKT_SERVER_UPDATE;
    pktServerUpdate *upd = (pktServerUpdate *)pkt->data;
    upd->tick = server->tick;
    upd->shot_count = server->shot_count;
    memcpy(upd->shots, server->shots, server->shot_count * sizeof(Shot));

    for (int c = 0; c < server->client_count; c++) {
        Entity *me = entity_from_client_id(server, c);

        if (me) {
            upd->your_player = *me;
        } else {
            memset(&upd->your_player, 0, sizeof(Entity));
        }
        upd->entity_count = 0;

        strcpy(upd->chat, server->chat);

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
    e->active = true;

    // Find PLAYER_START in map
    Vector3 spawn_pos = {0, 0, 0};
    for (int i = 0; i < server->map.entity_count; i++) {
        Entity *me = &server->map.entities[i];
        if (me->active && me->type == ENT_PLAYER_START) {
            spawn_pos = me->position;
            break;
        }
    }
    e->position = spawn_pos;

    strncpy(e->player.username, username, sizeof(e->player.username) - 1);
    e->player.health = 67; // funniest code ever

    server->client_count++;
    server->entity_count++;

    char message[32];
    snprintf(message, 32, "%s joined (%d players)\n", username,
             server->client_count);
    strcat(server->chat, message);
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
    if (cmd.jump_requested)
        server->jump_pending[client_id] = 1;
    server->last_client_updates[client_id] = cmd;

    for (int i = 0; i < cmd.shot_count && server->shot_count < MAX_SHOTS; i++) {
        server->shots[server->shot_count++] = cmd.shots[i];
    }
}

void sv_init(Server *server) {
    memset(server, 0, sizeof(Server));

    Entity npc = {0};
    npc.client_id = NOT_PLAYER;
    npc.position = Vector3One();
    npc.type = ENT_NPC_GENERIC;
    npc.active = true;

    server->entities[0] = npc;
    server->entity_count = 1;
}

void sv_tick(Server *server, float dt) {
    for (int i = 0; i < server->entity_count; i++) {
        Entity *e = &server->entities[i];
        if (!e->active)
            continue;

        if (e->client_id != NOT_PLAYER) {
            pktUserUpdate upd = server->last_client_updates[e->client_id];

            // Trust XZ from client only, never Y
            e->position.x = upd.position.x;
            e->position.z = upd.position.z;
            e->velocity.x = upd.current_velocity.x;
            e->velocity.z = upd.current_velocity.z;

            // Allow jump: client explicitly requested it and entity is on floor
            int on_floor =
                e->position.y <= 0.0f ||
                is_on_sector_floor(e->position, &server->map, STEP_HEIGHT);
            if (server->jump_pending[e->client_id] && on_floor) {
                e->velocity.y = 10.f;
                server->jump_pending[e->client_id] = 0;
            }
        }

        // Server owns Y
        e->velocity.y -= 20.0f * dt;
        e->velocity.x *= 0.8f;
        e->velocity.z *= 0.8f;

        e->position = Vector3Add(e->position, Vector3Scale(e->velocity, dt));

        // Floor collision
        apply_sector_collision(&e->position, &e->velocity,
                               &server->map, STEP_HEIGHT, 1);
        if (e->position.y < 0.0f) {
            e->position.y = 0.0f;
            e->velocity.y = 0.0f;
        }
    }
    server->tick++;
}
void sv_push_message(Server *sv, int client_id, char *message) {
    char line[128];
    snprintf(line, sizeof(line), "%s: %s\n", sv->clients[client_id].username,
             message);
    strncat(sv->chat, line, sizeof(sv->chat) - strlen(sv->chat) - 1);
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

            sv_join_player(sv, join->username, client_addr);

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
        case PKT_USER_MESSAGE: {
            if (id == -1)
                break;

            pktUserMessage *msg = (pktUserMessage *)pkt->data;
            char string[MAX_MSG_LEN];
            strncpy(string, msg->message, MAX_MSG_LEN - 1);
            string[MAX_MSG_LEN - 1] = '\0';
            sv_push_message(sv, id, string);
            printf("recieved message\n\n");
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


// TCP map transfer
typedef struct {
    Server *sv;
} TcpMapArgs;

void *tcp_map_thread(void *arg) {
    TcpMapArgs *args = (TcpMapArgs *)arg;
    Server *sv = args->sv;
    free(args);
    printf("[TCP] Starting TCP map thread\n");

    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("[TCP] TCP socket created: %d\n", tcp_fd);
    int reuse = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in tcp_addr = {0};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(4446);

    if (bind(tcp_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) == 0)
        printf("[TCP] Bound to port 4446\n");
    else
        printf("[TCP] Bind failed\n");
    listen(tcp_fd, 5);

    printf("[TCP] TCP map server listening on 4446\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        printf("[TCP] Waiting for map client...\n");
        fflush(stdout);
        int client_fd = accept(tcp_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("[TCP] Accept failed\n");
            fflush(stdout);
            continue;
        }
        printf("[TCP] Map client connected, sending map...\n");
        fflush(stdout);

        // Send map size then map data
        pthread_mutex_lock(&server_mutex);
        int map_size = sizeof(Map);
        printf("[TCP] Sending map size: %d\n", map_size);
        fflush(stdout);
        write(client_fd, &map_size, sizeof(map_size));
        printf("[TCP] Sending map data...\n");
        fflush(stdout);
        write(client_fd, &sv->map, map_size);
        pthread_mutex_unlock(&server_mutex);

        printf("[TCP] Map sent successfully\n");
        fflush(stdout);
        close(client_fd);
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

    printf("0ing server..\n");

    Server *sv = calloc(1, sizeof(Server));
    sv_init(sv);

    // Load map
    FILE *map_file = fopen("map.dat", "rb");
    if (map_file) {
        fread(&sv->map, sizeof(Map), 1, map_file);
        fclose(map_file);
        printf("Loaded map with %d sectors, %d entities\n",
               sv->map.sector_count, sv->map.entity_count);
        fflush(stdout);
    } else {
        printf("No map.dat found, using empty map\n");
        fflush(stdout);
    }

    // Start TCP map thread
    TcpMapArgs *tcp_args = malloc(sizeof(TcpMapArgs));
    tcp_args->sv = sv;
    pthread_t tcp_tid;
    pthread_create(&tcp_tid, NULL, tcp_map_thread, tcp_args);

    RecvThreadArgs *args = malloc(sizeof(RecvThreadArgs));
    args->listenfd = listenfd;
    args->sv = sv;

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
            sv_tick(sv, dt);
            sv_broadcast(sv, listenfd);
            pthread_mutex_unlock(&server_mutex);

            last = now;
        }
    }

    pthread_join(tid, NULL);
    free(sv);
    return 0;
}

