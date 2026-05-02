#include "game.h"
#include "global.h"
#include "map.h"
#include "protocol.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "string.h"
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>
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
        pthread_mutex_lock(&global->ingame.entity_mutex);

        memcpy(global->ingame.entities, upd->entities, sizeof(upd->entities));
        global->ingame.myself = upd->your_player;
        global->ingame.entity_count = upd->entity_count;
        strcpy(global->ingame.chat, upd->chat);

        pthread_mutex_unlock(&global->ingame.entity_mutex);
    }
    return NULL;
}

void connect_sv(Global *global) {
    int sockfd;
    struct sockaddr_in sv_addr;

    bzero(&sv_addr, sizeof(struct sockaddr_in));

    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(4445);
    inet_pton(AF_INET, "127.0.0.1", &sv_addr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int result = connect(sockfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr));

    uint8_t buf[sizeof(Packet) + sizeof(pktUserJoin)];

    pktUserJoin user_join = {0};
    strcpy(user_join.username, "gael");

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
                global->ingame.input_state = IS_MOVING;
                global->ingame.message_len = 0;
                strcpy(global->ingame.message, "ess");
                global->gamemode = GM_INGAME;

                global->ingame.map = load_map("map.map");

                global->ingame.default_font = LoadFont("assets/fonts/font.ttf");

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

void host() {
    pid_t pid = fork();

    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        execl("./server", "server", NULL);
        exit(1);
    }
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

    switch (state->input_state) {
    case IS_CHAT:
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->input_state = IS_MOVING;
            break;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (state->message_len > 0) {
                state->message_len--;
                state->message[state->message_len] =
                    '\0'; // null AFTER decrement
            }
            break;
        }
        int character = GetCharPressed();
        if (character > 0 && state->message_len < 32 - 1) {
            state->message[state->message_len] = (char)character;
            state->message_len++;
            state->message[state->message_len] = '\0'; // keep null-terminated
        }
        printf("%s\n", state->message);

        if (IsKeyPressed(KEY_ENTER)) {
            state->input_state = IS_MOVING;
            uint8_t um_buf[sizeof(Packet) + sizeof(pktUserMessage)];
            Packet *um_pkt = (Packet *)um_buf;
            um_pkt->type = PKT_USER_MESSAGE;
            pktUserMessage *um_data = (pktUserMessage *)um_pkt->data;
            memcpy(um_data->message, global->ingame.message, sizeof(char[32]));

            int sent = sendto(global->ingame.sockfd, um_buf, sizeof(um_buf), 0,
                              (struct sockaddr *)&global->ingame.sv_addr,
                              sizeof(global->ingame.sv_addr));
            printf("sendto returned: %d\n", sent);

            state->message[0] = '\0';
            state->message_len = 0;
        }

        break;
    case IS_MOVING:

        if (IsKeyPressed(KEY_T)) {
            state->input_state = IS_CHAT;
        }
        if (IsKeyDown(KEY_W))
            state->velocity = Vector3Add(
                state->velocity, Vector3Scale(forward, speed * frameTime));

        if (IsKeyDown(KEY_S))
            state->velocity = Vector3Add(
                state->velocity, Vector3Scale(forward, -speed * frameTime));

        if (IsKeyDown(KEY_A))
            state->velocity = Vector3Add(
                state->velocity, Vector3Scale(right, -speed * frameTime));

        if (IsKeyDown(KEY_D))
            state->velocity = Vector3Add(
                state->velocity, Vector3Scale(right, speed * frameTime));

        // if (IsKeyDown(KEY_O)) {
        //     printf("ds\n");

        //     uint8_t disconnect_buf[sizeof(Packet)];
        //     Packet *disconnect_pkt = (Packet *)disconnect_buf;
        //     disconnect_pkt->type = PKT_USER_DISCONNECT;

        //     sendto(global->ingame.sockfd, disconnect_buf,
        //            sizeof(disconnect_buf), 0,
        //            (struct sockaddr *)&global->ingame.sv_addr,
        //            sizeof(global->ingame.sv_addr));
        // }

        if (IsKeyDown(KEY_U)) {
            ShowCursor();
        }
        if (IsKeyDown(KEY_I)) {
            DisableCursor();
        }
        break;
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

    ClearBackground(BLACK);
    DrawFPS(10, 10);

    float ry = state->yaw * DEG2RAD;
    float rp = state->pitch * DEG2RAD;
    Vector2 mouseDelta = GetMouseDelta();

    float sensitivity = 0.1f;

    if (state->input_state == IS_MOVING) {
        state->yaw -= mouseDelta.x * sensitivity;
        state->pitch -= mouseDelta.y * sensitivity;
    }
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

    for (int i = 0; i < count; i++) {

        render_net_entity(&camera, &global->assets, snapshot[i]);
    }
    EndMode3D();

    char text[64];
    snprintf(text, sizeof(text), "health: %d", state->myself.player.health);

    char preview[32];
    snprintf(preview, sizeof(preview), "gael: %s", state->message);
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    const char *chat = global->ingame.chat;
    const char *lines[3] = {NULL, NULL, NULL};
    const char *p = chat + strlen(chat);
    int found = 0;
    while (p > chat && found < 3) {
        p--;
        if (*p == '\n') {
            lines[found++] = p + 1;
        }
    }
    if (found < 3)
        lines[found++] = chat;

    if (state->input_state == IS_CHAT) {
        int fontSize = 22;
        int padX = 10;
        int padY = 8;
        int lineH = 26;
        int maxLines = 5;
        int boxW = sw / 2;
        int boxX = 20;
        int inputH = fontSize + padY * 2;
        int chatAreaH = lineH * maxLines + padY;
        int totalH = chatAreaH + inputH + 4;
        int boxY = sh - totalH - 20;

        DrawRectangle(boxX, boxY, boxW, totalH, (Color){0, 0, 0, 160});

        const char *chat = global->ingame.chat;
        const char *lines[5] = {0};
        int found = 0;
        const char *p = chat + strlen(chat);
        while (p > chat && found < maxLines) {
            p--;
            if (*p == '\n')
                lines[found++] = p + 1;
        }
        if (found < maxLines)
            lines[found++] = chat;

        for (int i = found - 1; i >= 0; i--) {
            char line[128];
            const char *end = strchr(lines[i], '\n');
            int len = end ? (int)(end - lines[i]) : (int)strlen(lines[i]);
            snprintf(line, sizeof(line), "%.*s", len, lines[i]);
            int lineY = boxY + padY + (found - 1 - i) * lineH;
            DrawTextEx(global->ingame.default_font, line,
                       (Vector2){boxX + padX, lineY}, 20, 0, WHITE);
        }

        // input row at the bottom of the panel
        int inputY = boxY + chatAreaH + 4;
        DrawRectangle(boxX, inputY, boxW, inputH, (Color){30, 30, 30, 220});
        DrawRectangleLines(boxX, inputY, boxW, inputH,
                           (Color){180, 180, 180, 160});
        DrawTextEx(global->ingame.default_font, preview,
                   (Vector2){boxX + padX, inputY + padY}, fontSize, 0, WHITE);
    } else {
        const char *chat = global->ingame.chat;
        const char *lines[3] = {0};
        int found = 0;
        const char *p = chat + strlen(chat);
        while (p > chat && found < 3) {
            p--;
            if (*p == '\n')
                lines[found++] = p + 1;
        }
        if (found < 3)
            lines[found++] = chat;

        for (int i = found - 1; i >= 0; i--) {
            char line[128];
            const char *end = strchr(lines[i], '\n');
            int len = end ? (int)(end - lines[i]) : (int)strlen(lines[i]);
            snprintf(line, sizeof(line), "%.*s", len, lines[i]);
            DrawTextEx(global->ingame.default_font, line,
                       (Vector2){20, sh - 120 + (found - 1 - i) * 26}, 20, 0,
                       (Color){255, 255, 255, 180});
        }
    }
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
