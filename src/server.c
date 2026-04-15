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
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#define PORT 4445

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int listenfd;
    Server *sv;
} RecvThreadArgs;

typedef struct {
    int listenfd;
    int client_id;
    struct sockaddr_in client_addr;
    Server *sv;
} ClientThreadArgs;

void *client_recv_thread(void *arg) {
    ClientThreadArgs *args = (ClientThreadArgs *)arg;
    Server *sv = args->sv;
    int listenfd = args->listenfd;
    int client_id = args->client_id;
    struct sockaddr_in client_addr = args->client_addr;
    socklen_t client_len = sizeof(client_addr);
    free(args);

    for (;;) {
        uint8_t buf[sizeof(Packet) + sizeof(pktUserUpdate)];
        int n = recvfrom(listenfd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
            continue;

        Packet *pkt = (Packet *)buf;

        // finish the rest of packet types
        if (pkt->type == PKT_USER_UPDATE) {
            pktUserUpdate *upd = (pktUserUpdate *)pkt->data;
            pthread_mutex_lock(&server_mutex);
            sv_receive_update(sv, client_id, *upd);
            pthread_mutex_unlock(&server_mutex);
        } else if (pkt->type == PKT_USER_DISCONNECT) {
            pthread_mutex_lock(&server_mutex);
            sv_delete_player(sv, client_id);
            pthread_mutex_unlock(&server_mutex);
            break; // exits the loop, thread ends
        }
    }
    return NULL;
}

void *recv_thread(void *arg) {
    RecvThreadArgs *args = (RecvThreadArgs *)arg;
    Server *sv = args->sv;
    int listenfd = args->listenfd;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (;;) {
        Packet *pkt;
        uint8_t buf[sizeof(Packet) + sizeof(pktUserJoin)];
        int n = recvfrom(listenfd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
            continue;

        printf("received something\n");

        pkt = (Packet *)buf;
        if (pkt->type == PKT_USER_JOIN) {
            pktUserJoin *join = (pktUserJoin *)pkt->data;
            printf("user joined: %s\n", join->userName);

            uint8_t ack_buf[sizeof(Packet) + sizeof(pktUserJoinAck)];
            Packet *ack_pkt = (Packet *)ack_buf;
            ack_pkt->type = PKT_USER_JOIN_ACK;
            pktUserJoinAck *ack_data = (pktUserJoinAck *)ack_pkt->data;
            ack_data->accepted = true;

            printf("sending ack...\n");
            sendto(listenfd, ack_buf, sizeof(ack_buf), 0,
                   (struct sockaddr *)&client_addr, client_len);
            printf("sent ack\n");

            pthread_mutex_lock(&server_mutex);
            sv_join_player(sv, join->userName, client_addr);
            int client_id = sv->client_count - 1;
            pthread_mutex_unlock(&server_mutex);

            // Spawn a dedicated recv thread for this client
            ClientThreadArgs *cargs = malloc(sizeof(ClientThreadArgs));
            cargs->listenfd = listenfd;
            cargs->client_id = client_id;
            cargs->client_addr = client_addr;
            cargs->sv = sv;

            pthread_t tid;
            pthread_create(&tid, NULL, client_recv_thread, cargs);
            pthread_detach(tid);
        }
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
    int result = bind(listenfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr));
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
            pthread_mutex_lock(&server_mutex);
            sv_tick(&sv, elapsed);
            pthread_mutex_unlock(&server_mutex);
            last_tick = now;
        }
    }

    pthread_join(recv_tid, NULL);
}

void sv_receive_update(Server *server, int client_id, pktUserUpdate cmd) {
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
            pktUserUpdate cmd = server->last_client_updates[e->client_id];
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

void sv_join_player(Server *server, const char username[],
                    struct sockaddr_in client_sock_addr) {
    const int entity_count = server->entity_count;
    strncpy(server->entities[0].player.username, username,
            sizeof(server->entities[0].player.username) - 1);
    server->entities[entity_count].type = ENT_PLAYER;
    server->entities[entity_count].client_id = server->client_count;
    server->entities[entity_count].position = (Vector3){0, 0, 0};
    server->entities[entity_count].active = true;

    Client new_client = (Client){0};
    strcpy(new_client.username, username);
    new_client.client_id = server->client_count;
    new_client.sockaddr = client_sock_addr;
    server->clients[server->client_count] = new_client;

    server->client_count++;
    server->entity_count++;

    printf("%s joined the game, players: %d\n", username, server->client_count);
}
void sv_delete_player(Server *server, int client_id) {
    // Deactivate the entity belonging to this client
    for (int i = 0; i < server->entity_count; i++) {
        if (server->entities[i].client_id == client_id) {
            server->entities[i].active = false;
            server->entities[i].client_id = -1;
            break;
        }
    }

    // Clear the client slot
    Client *c = &server->clients[client_id];
    printf("%s left the game, players: %d\n", c->username,
           server->client_count - 1);
    memset(c, 0, sizeof(Client));

    // Clear their last update
    memset(&server->last_client_updates[client_id], 0, sizeof(pktUserUpdate));

    server->client_count--;
}
