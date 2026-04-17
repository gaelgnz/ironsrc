#include "assets.h"
#include "entity.h"
#include "protocol.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "stdio.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
void render_entity(Camera *camera, Assets *assets, Entity entity) {
    if (entity.type == ENT_NPC_GENERIC) {
        printf("%f", entity.position.x);
        DrawBillboard(*camera, get_texture(assets, "player"), entity.position,
                      1., WHITE);
        DrawCubeWires(entity.position, 30.f, 30.f, 30.f, BLACK);
    } else {
    }
}

typedef enum GameMode {
    GM_MENU,
    GM_CONNECTING,
    GM_INGAME,
} GameMode;

typedef struct IngameState {
    Vector3 position, velocity;
    float yaw, pitch;
    int sockfd;
    struct sockaddr_in sv_addr;

    // server state
    NetEntity entities[256];
    int entity_count;
    pthread_mutex_t entity_mutex;
} IngameState;

typedef struct Global {
    GameMode gamemode;
    Assets assets;
    union {
        IngameState ingame;
    };
} Global;
typedef struct {
    Global *global;
} RecvArgs;

void *client_recv_thread(void *arg) {
    Global *global = ((RecvArgs *)arg)->global;
    free(arg);

    uint8_t buf[sizeof(Packet) + sizeof(pktServerUpdate)];

    for (;;) {
        int n = recv(global->ingame.sockfd, buf, sizeof(buf), 0);
        if (n < 0)
            continue;

        Packet *pkt = (Packet *)buf;
        if (pkt->type != PKT_SERVER_UPDATE)
            continue;

        pktServerUpdate *upd = (pktServerUpdate *)pkt->data;
        printf("got server update: %d entities\n", upd->entity_count);
        pthread_mutex_lock(&global->ingame.entity_mutex);
        memcpy(global->ingame.entities, upd->entities, sizeof(upd->entities));
        global->ingame.entity_count = upd->entity_count;
        pthread_mutex_unlock(&global->ingame.entity_mutex);
    }
    return NULL;
}
inline void *pack_packet_typed(void *buf, int type, const void *payload,
                               size_t size) {
    Packet *pkt = (Packet *)buf;
    pkt->type = type;
    memcpy(pkt->data, payload, size);
    return buf;
}
void connect_sv(Global *global) {
    int sockfd;
    struct sockaddr_in sv_addr;

    bzero(&sv_addr, sizeof(struct sockaddr_in));

    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(4445);
    inet_pton(AF_INET, "127.0.0.1", &sv_addr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    connect(sockfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr));

    uint8_t buf[sizeof(Packet) + sizeof(pktUserJoin)];

    pktUserJoin user_join = {0};
    strcpy(user_join.userName, "gael");

    pack_packet_typed(buf, PKT_USER_JOIN, &user_join, sizeof(user_join));

    send(sockfd, buf, sizeof(buf), 0);
    struct timeval tv = {0};
    tv.tv_sec = 2; // 2 sec timeout

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf2[sizeof(Packet) + sizeof(pktUserJoinAck)];

    printf("expecting userjoinack\n");
    int n = recv(sockfd, buf, sizeof(buf), 0);

    if (n > 0) {
        Packet *pkt = (Packet *)buf;

        if (pkt->type == PKT_USER_JOIN_ACK) {
            pktUserJoinAck *ack = (pktUserJoinAck *)pkt->data;

            if (ack->accepted) {
                printf("accepted\n");
                pthread_mutex_init(&global->ingame.entity_mutex, NULL);
                global->ingame.sockfd = sockfd;
                global->ingame.sv_addr = sv_addr;
                global->gamemode = GM_INGAME;
                RecvArgs *rargs = malloc(sizeof(RecvArgs));
                rargs->global = global;
                pthread_t tid;
                pthread_create(&tid, NULL, client_recv_thread, rargs);
                pthread_detach(tid);
                printf("recv thread created\n");
                printf("recv thread created\n");
            }
        }
    }
}

void menu_loop(Global *global) {

    BeginDrawing();
    ClearBackground(WHITE);
    EndDrawing();
    connect_sv(global);
}

void game_loop(Global *global) {
    IngameState *state = &global->ingame;

    float frameTime = GetFrameTime();
    if (frameTime > 0.25f)
        frameTime = 0.25f;

    float speed = 8000.0f * GetFrameTime();

    Vector3 forward = {sinf(state->yaw * DEG2RAD), 0,
                       cosf(state->yaw * DEG2RAD)};

    Vector3 right = {-cosf(state->yaw * DEG2RAD), 0,
                     sinf(state->yaw * DEG2RAD)};

    if (IsKeyDown(KEY_W))
        state->velocity = Vector3Add(state->velocity,
                                     Vector3Scale(forward, speed * frameTime));

    if (IsKeyDown(KEY_S))
        state->velocity = Vector3Add(state->velocity,
                                     Vector3Scale(forward, -speed * frameTime));

    if (IsKeyDown(KEY_A))
        state->velocity = Vector3Add(state->velocity,
                                     Vector3Scale(right, -speed * frameTime));

    if (IsKeyDown(KEY_D))
        state->velocity =
            Vector3Add(state->velocity, Vector3Scale(right, speed * frameTime));

    if (IsKeyDown(KEY_O)) {
        printf("ds\n");

        uint8_t disconnect_buf[sizeof(Packet)];
        Packet *disconnect_pkt = (Packet *)disconnect_buf;
        disconnect_pkt->type = PKT_USER_DISCONNECT;

        sendto(global->ingame.sockfd, disconnect_buf, sizeof(disconnect_buf), 0,
               (struct sockaddr *)&global->ingame.sv_addr,
               sizeof(global->ingame.sv_addr));
    }

    if (IsKeyDown(KEY_U)) {
    }
    state->velocity.y -= 20.0f * frameTime;

    if (state->position.y < 0.0f) {
        state->position.y = 0.0f;
        state->velocity.y = 0.0f;
    }

    state->position =
        Vector3Add(state->position, Vector3Scale(state->velocity, frameTime));

    state->velocity.x *= 0.8f;
    state->velocity.z *= 0.8f;

    if (IsKeyPressed(KEY_SPACE))
        state->velocity.y -= 10.0f;

    BeginDrawing();
    ClearBackground(BLUE);
    DrawFPS(10, 10);

    float ry = state->yaw * DEG2RAD;
    float rp = state->pitch * DEG2RAD;
    Vector2 mouseDelta = GetMouseDelta();

    float sensitivity = 0.1f;

    state->yaw -= mouseDelta.x * sensitivity;
    state->pitch -= mouseDelta.y * sensitivity;

    // clamp pitch so you don't flip
    if (state->pitch > 89.0f)
        state->pitch = 89.0f;
    if (state->pitch < -89.0f)
        state->pitch = -89.0f;
    Camera3D camera = {0};
    camera.position = Vector3Add(state->position, (Vector3){0, 1.0f, 0});

    camera.target = (Vector3){camera.position.x + sinf(ry) * cosf(rp),
                              camera.position.y + sinf(rp),
                              camera.position.z + cosf(ry) * cosf(rp)};

    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 120.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);

    DrawCubeTexture(get_texture(&global->assets, "brick_01"),
                    (Vector3){0, -0.5f, 0}, 100.0f, 1.0f, 100.0f, WHITE);
    // snapshot to avoid holding lock during render
    pthread_mutex_lock(&state->entity_mutex);
    NetEntity snapshot[256];
    int count = state->entity_count;
    memcpy(snapshot, state->entities, count * sizeof(NetEntity));
    pthread_mutex_unlock(&state->entity_mutex);

    BeginMode3D(camera);
    for (int i = 0; i < count; i++) {
        if (snapshot[i].active)
            printf("rendering %d entities\n", count);
        render_net_entity(&camera, &global->assets, snapshot[i]);
    }
    EndMode3D();
    EndMode3D();

    // DrawTextEx(global->font, "IRONSRC ENGINE - SERVER TICK: 60Hz",
    //            (Vector2){20, 20}, 30, 0, WHITE);

    EndDrawing();

    pktUserUpdate user_update = {0};
    user_update.current_velocity = state->velocity;
    user_update.position = state->position;

    uint8_t uu_buf[sizeof(Packet) + sizeof(pktUserUpdate)];
    Packet *uu_pkt = (Packet *)uu_buf;
    uu_pkt->type = PKT_USER_UPDATE;
    pktUserUpdate *uu_data = (pktUserUpdate *)uu_pkt->data;
    uu_data->current_velocity = global->ingame.velocity;
    uu_data->position = global->ingame.position;
    sendto(global->ingame.sockfd, uu_buf, sizeof(uu_buf), 0,
           (struct sockaddr *)&global->ingame.sv_addr,
           sizeof(global->ingame.sv_addr));
}

int main(void) {

    InitWindow(800, 600, "IronSrc test");
    SetTargetFPS(144);

    Global global = {0};
    global.gamemode = GM_MENU;
    global.assets = assets_load();

    Font font = LoadFont("assets/font.ttf");

    const float dt = 1.0f / 60.0f;

    DisableCursor();

    Entity local_entities[256]; // render only

    while (!WindowShouldClose()) {
        if (WindowShouldClose()) {
            if (global.gamemode == GM_INGAME) {
                printf("ds\n");

                uint8_t disconnect_buf[sizeof(Packet)];
                Packet *disconnect_pkt = (Packet *)disconnect_buf;
                disconnect_pkt->type = PKT_USER_DISCONNECT;

                sendto(global.ingame.sockfd, disconnect_buf,
                       sizeof(disconnect_buf), 0,
                       (struct sockaddr *)&global.ingame.sv_addr,
                       sizeof(global.ingame.sv_addr));
            }
            break;
        }
        switch (global.gamemode) {
        case GM_MENU:
            menu_loop(&global);
            break;
        case GM_INGAME:
            game_loop(&global);
            break;
        }
    }

    CloseWindow();
    return 0;
}
