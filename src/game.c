#include "game.h"
#include "entity.h"
#include "global.h"
#include "map.h"
#include "protocol.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "server.h"
#include "string.h"
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#define GROUND_ACCEL 10.0f
#define AIR_ACCEL 2.0f
#define MAX_SPEED 10.0f
#define GROUND_FRICTION 8.0f
#define GRAVITY 20.0f
#define JUMP_VEL 10.0f
#define CROUCH_SPEED_MULT 0.4f
#define CROUCH_CAMERA_HEIGHT 0.5f
#define NORMAL_CAMERA_HEIGHT 1.0f

typedef struct {
    Global *global;
} RecvArgs;

static void play_chat_sound(IngameState *state) {
    if (state->chat_sound_loaded && IsSoundPlaying(state->chat_sound))
        StopSound(state->chat_sound);
    if (state->chat_sound_loaded)
        PlaySound(state->chat_sound);
}

void *client_recv_thread(void *arg) {
    Global *global = ((RecvArgs *)arg)->global;
    free(arg);

    uint8_t buf[sizeof(Packet) + sizeof(pktServerUpdate)];
    char last_chat[sizeof(global->ingame.chat)];
    memset(last_chat, 0, sizeof(last_chat));

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

        if (strcmp(upd->chat, last_chat) != 0) {
            play_chat_sound(&global->ingame);
        }
        strcpy(global->ingame.chat, upd->chat);
        memcpy(last_chat, upd->chat, sizeof(last_chat));

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

    struct timeval tv = {0};
    tv.tv_usec = 200000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t join_buf[sizeof(Packet) + sizeof(pktUserJoin)];
    pktUserJoin user_join = {0};
    strcpy(user_join.username, "gael");

    pack_packet_typed(join_buf, PKT_USER_JOIN, &user_join, sizeof(user_join));

    uint8_t recv_buf[sizeof(Packet) + sizeof(pktServerUpdate)];
    int connected = 0;
    for (int attempt = 0; attempt < 20 && !connected; attempt++) {
        sendto(sockfd, join_buf, sizeof(join_buf), 0,
               (struct sockaddr *)&sv_addr, sizeof(sv_addr));

        int n = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (n > 0) {
            Packet *pkt = (Packet *)recv_buf;
            if (pkt->type == PKT_USER_JOIN_ACK) {
                pktUserJoinAck *ack = (pktUserJoinAck *)pkt->data;
                if (ack->accepted) {
                    connected = 1;
                    break;
                }
            }
        }
    }

    if (!connected)
        return;

    pthread_mutex_init(&global->ingame.entity_mutex, NULL);
    global->ingame.sockfd = sockfd;
    global->ingame.sv_addr = sv_addr;
    global->ingame.input_state = IS_MOVING;
    global->ingame.message_len = 0;
    strcpy(global->ingame.message, "ess");
    global->gamemode = GM_INGAME;
    global->ingame.crouching = 0;

    global->ingame.map = load_map("map.map");

    if (FileExists("chat.wav")) {
        InitAudioDevice();
        global->ingame.chat_sound = LoadSound("chat.wav");
        global->ingame.chat_sound_loaded =
            global->ingame.chat_sound.stream.buffer != NULL;
    } else {
        global->ingame.chat_sound_loaded = 0;
    }

    RecvArgs *rargs = malloc(sizeof(RecvArgs));
    rargs->global = global;
    pthread_t tid;
    pthread_create(&tid, NULL, client_recv_thread, rargs);
    pthread_detach(tid);
}

void host() {
    pid_t pid = fork();

    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        execl("./server", "server", NULL);
        exit(1);
    }
}

static void apply_acceleration(Vector3 *velocity, Vector3 wishdir,
                               float wishspeed, float accel, float frametime) {
    float currentspeed = Vector3DotProduct(*velocity, wishdir);
    float addspeed = wishspeed - currentspeed;

    if (addspeed <= 0)
        return;

    float accelspeed = accel * wishspeed * frametime;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    velocity->x += accelspeed * wishdir.x;
    velocity->y += accelspeed * wishdir.y;
    velocity->z += accelspeed * wishdir.z;
}

static void apply_ground_friction(Vector3 *velocity, float friction,
                                  float frametime) {
    float speed = sqrtf(velocity->x * velocity->x + velocity->z * velocity->z);
    if (speed < 0.01f)
        return;

    float drop = speed * friction * frametime;
    float newspeed = speed - drop;
    if (newspeed < 0)
        newspeed = 0;
    newspeed /= speed;

    velocity->x *= newspeed;
    velocity->z *= newspeed;
}

void game_loop(Global *global) {
    IngameState *state = &global->ingame;

    float frameTime = GetFrameTime();
    if (frameTime > 0.25f)
        frameTime = 0.25f;

    float ry = state->yaw * DEG2RAD;
    float rp = state->pitch * DEG2RAD;
    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.1f;

    if (state->input_state == IS_MOVING) {
        state->yaw -= mouseDelta.x * sensitivity;
        state->pitch -= mouseDelta.y * sensitivity;
    }
    if (state->pitch > 89.0f)
        state->pitch = 89.0f;
    if (state->pitch < -89.0f)
        state->pitch = -89.0f;

    Vector3 forward = {sinf(state->yaw * DEG2RAD), 0,
                       cosf(state->yaw * DEG2RAD)};
    Vector3 right = {-cosf(state->yaw * DEG2RAD), 0,
                     sinf(state->yaw * DEG2RAD)};

    int crouch_pressed =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    state->crouching = crouch_pressed;

    float speed_mult = state->crouching ? CROUCH_SPEED_MULT : 1.0f;
    float camera_height =
        state->crouching ? CROUCH_CAMERA_HEIGHT : NORMAL_CAMERA_HEIGHT;

    switch (state->input_state) {
    case IS_CHAT:
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->input_state = IS_MOVING;
            break;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (state->message_len > 0) {
                state->message_len--;
                state->message[state->message_len] = '\0';
            }
            break;
        }
        int character = GetCharPressed();
        if (character > 0 && state->message_len < 32 - 1) {
            state->message[state->message_len] = (char)character;
            state->message_len++;
            state->message[state->message_len] = '\0';
        }
        printf("%s\n", state->message);

        if (IsKeyPressed(KEY_ENTER)) {
            state->input_state = IS_MOVING;
            uint8_t um_buf[sizeof(Packet) + sizeof(pktUserMessage)];
            Packet *um_pkt = (Packet *)um_buf;
            um_pkt->type = PKT_USER_MESSAGE;
            pktUserMessage *um_data = (pktUserMessage *)um_pkt->data;
            memcpy(um_data->message, global->ingame.message, sizeof(char[32]));

            sendto(global->ingame.sockfd, um_buf, sizeof(um_buf), 0,
                   (struct sockaddr *)&global->ingame.sv_addr,
                   sizeof(global->ingame.sv_addr));

            state->message[0] = '\0';
            state->message_len = 0;
        }
        break;

    case IS_MOVING:
        if (IsKeyPressed(KEY_T)) {
            state->input_state = IS_CHAT;
        }
        if (IsKeyDown(KEY_U)) {
            ShowCursor();
        }
        if (IsKeyDown(KEY_I)) {
            DisableCursor();
        }

        Vector3 wishdir = {0};
        float wishspeed = MAX_SPEED * speed_mult;

        if (IsKeyDown(KEY_W)) {
            wishdir.x += forward.x;
            wishdir.z += forward.z;
        }
        if (IsKeyDown(KEY_S)) {
            wishdir.x -= forward.x;
            wishdir.z -= forward.z;
        }
        if (IsKeyDown(KEY_A)) {
            wishdir.x -= right.x;
            wishdir.z -= right.z;
        }
        if (IsKeyDown(KEY_D)) {
            wishdir.x += right.x;
            wishdir.z += right.z;
        }

        float wishlen = sqrtf(wishdir.x * wishdir.x + wishdir.z * wishdir.z);
        if (wishlen > 0) {
            wishdir.x /= wishlen;
            wishdir.z /= wishlen;
        }

        int on_ground = state->position.y <= 0.0f;

        if (on_ground) {
            apply_acceleration(&state->velocity, wishdir, wishspeed,
                               GROUND_ACCEL, frameTime);
            apply_ground_friction(&state->velocity, GROUND_FRICTION, frameTime);
        } else {
            apply_acceleration(&state->velocity, wishdir, wishspeed, AIR_ACCEL,
                               frameTime);
        }

        break;
    }

    uint8_t jump_requested = 0;
    if (IsKeyPressed(KEY_SPACE) && state->position.y <= 0.0f) {
        state->velocity.y = JUMP_VEL;
        jump_requested = 1;
    }

    state->velocity.y -= GRAVITY * frameTime;

    state->position =
        Vector3Add(state->position, Vector3Scale(state->velocity, frameTime));

    if (state->position.y < 0.0f) {
        state->position.y = 0.0f;
        state->velocity.y = 0.0f;
    }

    float hspeed = sqrtf(state->velocity.x * state->velocity.x +
                         state->velocity.z * state->velocity.z);
    if (hspeed > MAX_SPEED) {
        float scale = MAX_SPEED / hspeed;
        state->velocity.x *= scale;
        state->velocity.z *= scale;
    }

    BeginDrawing();
    ClearBackground(BLUE);
    DrawFPS(10, 10);

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    Camera3D camera = {0};
    camera.position =
        Vector3Add(state->position, (Vector3){0, camera_height, 0});
    camera.target = (Vector3){camera.position.x + sinf(ry) * cosf(rp),
                              camera.position.y + sinf(rp),
                              camera.position.z + cosf(ry) * cosf(rp)};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 120.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);
    DrawCubeTexture(get_texture(&global->assets, "dirt_01"),
                    (Vector3){0, -0.5f, 0}, 100.0f, 1.0f, 100.0f, WHITE);

    pthread_mutex_lock(&state->entity_mutex);
    NetEntity snapshot[MAX_ENTITIES];
    int count = state->entity_count;
    memcpy(snapshot, state->entities, count * sizeof(NetEntity));
    pthread_mutex_unlock(&state->entity_mutex);

    for (int i = 0; i < count; i++) {
        if (!snapshot[i].active || snapshot[i].type != ENT_PLAYER)
            continue;
        render_net_entity(&camera, &global->assets, snapshot[i], global);
    }
    EndMode3D();

    int cross_size = 15;
    int cross_gap = 5;
    int cross_cx = sw / 2;
    int cross_cy = sh / 2;

    DrawLineEx((Vector2){(float)(cross_cx - cross_size), (float)cross_cy},
               (Vector2){(float)(cross_cx - cross_gap), (float)cross_cy}, 3.f,
               WHITE);
    DrawLineEx((Vector2){(float)(cross_cx + cross_gap), (float)cross_cy},
               (Vector2){(float)(cross_cx + cross_size), (float)cross_cy}, 3.f,
               WHITE);
    DrawLineEx((Vector2){(float)cross_cx, (float)(cross_cy - cross_size)},
               (Vector2){(float)cross_cx, (float)(cross_cy - cross_gap)}, 3.f,
               WHITE);
    DrawLineEx((Vector2){(float)cross_cx, (float)(cross_cy + cross_gap)},
               (Vector2){(float)cross_cx, (float)(cross_cy + cross_size)}, 3.f,
               WHITE);

    for (int i = 0; i < count; i++) {
        if (!snapshot[i].active || snapshot[i].type != ENT_PLAYER)
            continue;
        draw_username_billboard(camera, global->assets.default_font,
                                snapshot[i].position,
                                snapshot[i].player.username);
    }

    const char *chat = global->ingame.chat;
    const char *chat_lines[3] = {NULL, NULL, NULL};
    const char *p = chat + strlen(chat);
    int found = 0;
    while (p > chat && found < 3) {
        p--;
        if (*p == '\n')
            chat_lines[found++] = p + 1;
    }
    if (found < 3)
        chat_lines[found++] = chat;

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

        const char *cl[5] = {0};
        int fc = 0;
        const char *cp = chat + strlen(chat);
        while (cp > chat && fc < maxLines) {
            cp--;
            if (*cp == '\n')
                cl[fc++] = cp + 1;
        }
        if (fc < maxLines)
            cl[fc++] = chat;

        for (int i = fc - 1; i >= 0; i--) {
            char line[128];
            const char *end = strchr(cl[i], '\n');
            int len = end ? (int)(end - cl[i]) : (int)strlen(cl[i]);
            snprintf(line, sizeof(line), "%.*s", len, cl[i]);
            int lineY = boxY + padY + (fc - 1 - i) * lineH;
            DrawTextEx(global->assets.default_font, line,
                       (Vector2){boxX + padX, lineY}, 20, 0, WHITE);
        }

        int inputY = boxY + chatAreaH + 4;
        DrawRectangle(boxX, inputY, boxW, inputH, (Color){30, 30, 30, 220});
        DrawRectangleLines(boxX, inputY, boxW, inputH,
                           (Color){180, 180, 180, 160});

        char preview[32];
        snprintf(preview, sizeof(preview), "gael: %s", state->message);
        DrawTextEx(global->assets.default_font, preview,
                   (Vector2){boxX + padX, inputY + padY}, fontSize, 0, WHITE);
    } else {
        for (int i = found - 1; i >= 0; i--) {
            char line[128];
            const char *end = strchr(chat_lines[i], '\n');
            int len =
                end ? (int)(end - chat_lines[i]) : (int)strlen(chat_lines[i]);
            snprintf(line, sizeof(line), "%.*s", len, chat_lines[i]);
            DrawTextEx(global->assets.default_font, line,
                       (Vector2){20, sh - 120 + (found - 1 - i) * 26}, 20, 0,
                       (Color){255, 255, 255, 180});
        }
    }

    EndDrawing();

    pktUserUpdate user_update = {0};
    user_update.current_velocity = state->velocity;
    user_update.position = state->position;
    user_update.jump_requested = jump_requested;

    uint8_t uu_buf[sizeof(Packet) + sizeof(pktUserUpdate)];
    Packet *uu_pkt = (Packet *)uu_buf;
    uu_pkt->type = PKT_USER_UPDATE;
    pktUserUpdate *uu_data = (pktUserUpdate *)uu_pkt->data;
    uu_data->current_velocity = global->ingame.velocity;
    uu_data->position = global->ingame.position;
    uu_data->jump_requested = jump_requested;
    sendto(global->ingame.sockfd, uu_buf, sizeof(uu_buf), 0,
           (struct sockaddr *)&global->ingame.sv_addr,
           sizeof(global->ingame.sv_addr));
}
