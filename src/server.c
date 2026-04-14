#include "server.h"
#include "entity.h"
#include "protocol.h"
#include "raymath.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <raylib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#define PORT 4445

typedef struct {
    int listenfd;
    Server *sv;
} RecvThreadArgs;

void *recv_thread(void *arg) {
    RecvThreadArgs *args = (RecvThreadArgs *)arg;

    Server *sv = args->sv;
    int *listenfd = &args->listenfd;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (;;) {
        Packet *pkt;
        uint8_t buf[sizeof(Packet) + sizeof(pktUserJoin)];
        int n = recvfrom(*listenfd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
            continue;

        pkt = (Packet *)buf;
        if (pkt->type == PKT_USER_JOIN) {
            pktUserJoin *join = (pktUserJoin *)pkt->data;
            printf("user joined: %s\n", join->userName);

            uint8_t ack_buf[sizeof(Packet) + sizeof(pktUserJoinAck)];
            Packet *ack_pkt = (Packet *)ack_buf;
            ack_pkt->type = PKT_USER_JOIN_ACK;

            pktUserJoinAck *ack_data = (pktUserJoinAck *)ack_pkt->data;
            ack_data->accepted = true;

            sendto(*listenfd, ack_buf, sizeof(ack_buf), 0,
                   (struct sockaddr *)&client_addr, client_len);

            ack_pkt->type = PKT_USER_JOIN_ACK;
        } else if (pkt->type == PKT_USER_UPDATE) {
        };
    }
    return NULL;
}

int main() {
    int listenfd;
    struct sockaddr_in sv_addr;
    bzero(&sv_addr, sizeof(sv_addr));
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    sv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sv_addr.sin_port = htons(PORT);
    sv_addr.sin_family = AF_INET;
    bind(listenfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr));
    printf("listening on 0.0.0.0:%d\n", PORT);

    Server sv = {0};
    sv_init(&sv);

    RecvThreadArgs args = {listenfd, &sv};
    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, recv_thread, &args);

    struct timespec now, last_tick;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last_tick.tv_sec) +
                         (now.tv_nsec - last_tick.tv_nsec) / 1e9;
        if (elapsed >= (1.0 / 20.0)) {
            sv_tick(&sv, elapsed);
            last_tick = now;
        }
    }

    pthread_join(recv_tid, NULL);
}

void sv_receive_update(Server *server, int client_id, UserCmd cmd) {

    server->last_client_updates[client_id] = cmd;
}

inline void sv_spawn_entity(Server *server, Entity entity) {
    if (server->entity_count >= MAX_ENTITIES)
        return;
    Entity *pEntitySlot = &server->entities[server->entity_count];
    *pEntitySlot = entity;
    pEntitySlot->client_id = -1;
    server->entity_count++;
}

void sv_tick(Server *server, float dt) {
    for (int i = 0; i < server->entity_count; i++) {
        Entity *e = &server->entities[i];
        if (!e->active)
            continue;

        if (e->client_id != -1) {
            UserCmd cmd = server->last_client_updates[e->client_id];

            e->position = cmd.position;
            e->velocity = cmd.current_velocity;
        }

        e->velocity.x *= 0.8f;
        e->velocity.z *= 0.8f;
        e->velocity.y -= 20.0f * dt;

        if (e->position.y < 0.0f) {
            e->position.y = 0.0f;
            e->velocity.y = 0.0f;
        }

        e->position = Vector3Add(e->position, Vector3Scale(e->velocity, dt));
    }
    server->tick++;
}
void sv_init(Server *server) {

    Entity entity = (Entity){0};
    entity.position = Vector3One();
    entity.active = true;
    entity.type = ENT_NPC_GENERIC;
    sv_spawn_entity(server, entity);
}

void sv_join_player(Server *server, const char username[]) {
    const int entity_count = server->entity_count;
    strncpy(server->entities[0].player.username, username,
            sizeof(server->entities[0].player.username) - 1);
    server->entities[entity_count].type = ENT_PLAYER;
    server->entities[entity_count].client_id = 0;
    server->entities[entity_count].position = (Vector3){0, 0, 0};
    server->entities[entity_count].active = true;

    server->entity_count++;
}
