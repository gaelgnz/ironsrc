/*
game.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "game.h"
#include "assets.h"
#include "entity.h"
#include "global.h"
#include "map.h"
#include "protocol.h"
#include "raygui.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "server.h"
#include "string.h"
#include "weapon.h"
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>
#define GROUND_ACCEL 10.0f
#define AIR_ACCEL 2.0f
#define MAX_SPEED 7.0f
#define GROUND_FRICTION 8.0f
#define GRAVITY 20.0f
#define JUMP_VEL 10.0f
#define CROUCH_SPEED_MULT 0.4f
#define CROUCH_CAMERA_HEIGHT 0.5f
#define NORMAL_CAMERA_HEIGHT 1.0f
#define STEP_HEIGHT 1.0f
#define BOB_SPEED 3.0f
#define BOB_AMP_X 6.0f
#define BOB_AMP_Y 3.0f

static float blocked_x = 0, blocked_z = 0;

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
    int last_health = 67;

    for (;;) {
        int n = recv(global->ingame.sockfd, buf, sizeof(buf), 0);
        if (n < 0)
            continue;

        Packet *pkt = (Packet *)buf;
        if (pkt->type != PKT_SERVER_UPDATE)
            continue;

        pktServerUpdate *upd = (pktServerUpdate *)pkt->data;
        pthread_mutex_lock(&global->ingame.entity_mutex);

        if (upd->your_player.player.health < last_health)
            global->ingame.damage_taken = 1;
        last_health = upd->your_player.player.health;

        memcpy(global->ingame.prev_entities, global->ingame.entities,
               sizeof(global->ingame.entities));
        global->ingame.prev_update_time = global->ingame.last_update_time;
        memset(global->ingame.entities, 0, sizeof(global->ingame.entities));
        memcpy(global->ingame.entities, upd->entities,
               upd->entity_count * sizeof(NetEntity));
        global->ingame.last_update_time = GetTime();
        global->ingame.myself = upd->your_player;
        global->ingame.entity_count = upd->entity_count;

        if (strcmp(upd->chat, last_chat) != 0) {
            play_chat_sound(&global->ingame);
        }
        strcpy(global->ingame.chat, upd->chat);
        memcpy(last_chat, upd->chat, sizeof(last_chat));

        global->ingame.server_tick = upd->tick;
        global->ingame.shot_count = upd->shot_count;
        memcpy(global->ingame.shots, upd->shots,
               upd->shot_count * sizeof(Shot));

        global->ingame.recv_sound_count = upd->sound_count;
        if (upd->sound_count > MAX_SOUNDS)
            global->ingame.recv_sound_count = MAX_SOUNDS;
        memcpy(global->ingame.recv_sounds, upd->sounds,
               global->ingame.recv_sound_count * sizeof(SoundWorld));

        pthread_mutex_unlock(&global->ingame.entity_mutex);
    }
    return NULL;
}

#define MAX_LOG_LINES 32
#define MAX_LOG_LINE_LEN 128
static char load_lines[MAX_LOG_LINES][MAX_LOG_LINE_LEN];
static int load_count = 0;

void game_log_init(void) {
    load_count = 0;
}

void game_log(const char *fmt, ...) {
    char buf[MAX_LOG_LINE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    int idx = load_count % MAX_LOG_LINES;
    strcpy(load_lines[idx], buf);
    load_count++;

    int display_count = load_count < MAX_LOG_LINES ? load_count : MAX_LOG_LINES;
    int start = load_count < MAX_LOG_LINES ? 0 : load_count - MAX_LOG_LINES;

    BeginDrawing();
    ClearBackground(BLACK);
    for (int i = 0; i < display_count; i++) {
        DrawText(load_lines[(start + i) % MAX_LOG_LINES], 10, 10 + i * 20, 20,
                 WHITE);
    }
    EndDrawing();
}

void connect_sv(Global *global, const char *map_name) {
    game_log_init();
    if (!map_name || !map_name[0])
        map_name = "map.dat";
    int sockfd;
    struct sockaddr_in sv_addr;

    bzero(&sv_addr, sizeof(struct sockaddr_in));

    sv_addr.sin_family = AF_INET;
    int port = global->menu.port[0] ? atoi(global->menu.port) : 4445;
    sv_addr.sin_port = htons(port);
    const char *ip = global->menu.ip[0] ? global->menu.ip : "127.0.0.1";
    inet_pton(AF_INET, ip, &sv_addr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    game_log("Creating UDP socket...");

    struct timeval tv = {0};
    tv.tv_usec = 200000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t join_buf[sizeof(Packet) + sizeof(pktUserJoin)];
    pktUserJoin user_join = {0};
    strcpy(user_join.username, "gael");

    pack_packet_typed(join_buf, PKT_USER_JOIN, &user_join, sizeof(user_join));

    uint8_t recv_buf[sizeof(Packet) + sizeof(pktServerUpdate)];
    int connected = 0;
    game_log("Connecting to %s:%d...", ip, port);
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

    game_log("Server accepted connection!");

    // Receive map via TCP
    game_log("Connecting to TCP map server on port 4446...");
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    game_log("TCP socket created: %d", tcp_fd);
    struct sockaddr_in tcp_addr = sv_addr;
    tcp_addr.sin_port = htons(4446);

    // Set timeout for connect
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(tcp_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(tcp_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(tcp_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) == 0) {
        game_log("TCP connected, receiving map...");
        int map_size;
        int n = read(tcp_fd, &map_size, sizeof(map_size));
        if (n != sizeof(map_size)) {
            game_log("Failed to read map size, got %d bytes", n);
            close(tcp_fd);
            global->ingame.map = load_map(map_name);
        } else {
            game_log("Received map size: %d bytes", map_size);
            global->ingame.map = malloc(map_size);
            int total_read = 0;
            while (total_read < map_size) {
                n = read(tcp_fd, (char *)global->ingame.map + total_read,
                         map_size - total_read);
                if (n <= 0) {
                    game_log("Failed to read map data, got %d (total %d/%d)",
                        n, total_read, map_size);
                    break;
                }
                total_read += n;
                game_log("Read %d bytes (total %d/%d)", n, total_read,
                       map_size);
            }
            close(tcp_fd);
            if (total_read == map_size) {
                game_log("Map received successfully");
                game_log("Map has %d sectors, %d entities",
                       global->ingame.map->sector_count,
                       global->ingame.map->entity_count);
            } else {
                game_log("Map receive incomplete, got %d/%d bytes",
                       total_read, map_size);
                free(global->ingame.map);
                global->ingame.map = load_map(map_name);
            }
        }
    } else {
        game_log("TCP connect failed, falling back to local file");
        global->ingame.map = load_map(map_name);
    }

    // Find PLAYER_START and set initial position
    if (global->ingame.map) {
        game_log("Map has %d sectors, %d entities",
               global->ingame.map->sector_count,
               global->ingame.map->entity_count);
        for (int i = 0; i < global->ingame.map->entity_count; i++) {
            Entity *e = &global->ingame.map->entities[i];
            if (e->active && e->type == ENT_PLAYER_START) {
                global->ingame.position = e->position;
                game_log("Spawning at PLAYER_START %.1f, %.1f, %.1f",
                       e->position.x, e->position.y, e->position.z);
                break;
            }
        }
    } else {
        game_log("No map loaded, using default position");
    }

    pthread_mutex_init(&global->ingame.entity_mutex, NULL);
    memset(global->ingame.prev_entities, 0, sizeof(global->ingame.prev_entities));
    global->ingame.last_update_time = GetTime();
    global->ingame.prev_update_time = global->ingame.last_update_time;
    global->ingame.sockfd = sockfd;
    global->ingame.sv_addr = sv_addr;
    global->ingame.input_state = IS_MOVING;
    global->ingame.yaw = 0;
    global->ingame.pitch = 0;
    global->ingame.velocity = (Vector3){0, 0, 0};
    global->ingame.message_len = 0;
    strcpy(global->ingame.message, "ess");
    global->gamemode = GM_INGAME;
    global->ingame.crouching = 0;
    global->ingame.inventory[0] = revolver();
    global->ingame.prev_health = 67;
    global->ingame.damage_taken = 0;
    global->ingame.hit_time = 0;
    global->ingame.kill_time = 0;
    global->ingame.server_tick = 0;
    global->ingame.bob_phase = 0;
    global->ingame.last_step_phase = 0;

    if (FileExists("chat.wav")) {
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

void host(const char *map_name) {
    if (!map_name || !map_name[0])
        map_name = "map.dat";
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("./server", "server", map_name, (char *)NULL);
        char path[256];
        snprintf(path, sizeof(path), "%s/server", getenv("HOME"));
        execl(path, "server", map_name, (char *)NULL);
        _exit(1);
    }
    // Wait for server to start
    usleep(500000); // 500ms
}

static int has_wall_in_dir(Vector3 pos, Map *map, Vector3 dir) {
    if (dir.x == 0 && dir.z == 0)
        return 0;
    Vector3 test = pos;
    test.x += dir.x * 0.2f;
    test.z += dir.z * 0.2f;
    for (int i = 0; i < map->sector_count; i++) {
        Sector *s = &map->sectors[i];
        if (test.x >= s->x && test.x <= s->x + s->width && test.z >= s->y &&
            test.z <= s->y + s->height) {
            if (test.y < (float)s->floor_height)
                return 1;
        }
    }
    return 0;
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
    if (newspeed < 0.01)
        newspeed = 0;
    newspeed /= speed;

    velocity->x *= newspeed;
    velocity->z *= newspeed;
}

static void handle_camera(IngameState *state, float *ry, float *rp,
                          Vector3 *forward, Vector3 *right) {
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

    *ry = state->yaw * DEG2RAD;
    *rp = state->pitch * DEG2RAD;

    forward->x = sinf(*ry);
    forward->z = cosf(*ry);
    right->x = -cosf(*ry);
    right->z = sinf(*ry);
}

static void shoot_weapon(IngameState *state, Weapon *weapon) {
    if (state->updatePkt.shot_count >= 16 || weapon->ammo <= 0)
        return;

    weapon->ammo--;
    float camera_height =
        state->crouching ? CROUCH_CAMERA_HEIGHT : NORMAL_CAMERA_HEIGHT;
    Vector3 start = Vector3Add(state->position, (Vector3){0, camera_height, 0});

    float ry = state->yaw * DEG2RAD;
    float rp = state->pitch * DEG2RAD;
    Vector3 forward = {sinf(ry) * cosf(rp), sinf(rp), cosf(ry) * cosf(rp)};
    Vector3 end = Vector3Add(start, Vector3Scale(forward, 1000.0f));

    if (state->map) {
        float wall_t = raycast_sectors(start, forward, 1000.0f, state->map);
        if (wall_t < 1000.0f) {
            end = Vector3Add(start, Vector3Scale(forward, wall_t));
        }
    }

    state->updatePkt.shots[state->updatePkt.shot_count++] = (Shot){
        .start = start,
        .end = end,
        .damage = 34,
        .spawn_tick = state->server_tick + 1,
    };

    if (state->updatePkt.sound_count < 5) {
        state->updatePkt.sounds[state->updatePkt.sound_count++] = (SoundWorld){
            .position = start,
            .sound_name = "pistol_shot",
        };
    }

    pthread_mutex_lock(&state->entity_mutex);
    for (int i = 0; i < state->entity_count; i++) {
        NetEntity *e = &state->entities[i];
        if (!e->active || (e->type != ENT_PLAYER && e->type != ENT_NPC_GENERIC))
            continue;

        Vector3 box_min = {e->position.x - 0.3f, e->position.y, e->position.z - 0.3f};
        Vector3 box_max = {e->position.x + 0.3f, e->position.y, e->position.z + 0.3f};

        Vector3 ray_dir = Vector3Subtract(end, start);
        float ray_len = Vector3Length(ray_dir);
        if (ray_len < 0.001f) break;
        ray_dir = Vector3Normalize(ray_dir);

        float tmin = -INFINITY, tmax = INFINITY;
        int hit = 1;
        for (int axis = 0; axis < 3; axis++) {
            float origin = ((float *)&start)[axis];
            float dir = ((float *)&ray_dir)[axis];
            float min = ((float *)&box_min)[axis];
            float max = ((float *)&box_max)[axis];

            if (fabsf(dir) < 0.0001f) {
                if (origin < min || origin > max) { hit = 0; break; }
            } else {
                float t1 = (min - origin) / dir;
                float t2 = (max - origin) / dir;
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) { hit = 0; break; }
            }
        }

        if (hit && tmin >= 0.0f && tmin <= ray_len) {
            state->hit_time = GetTime();
            int health = e->type == ENT_NPC_GENERIC ? e->npc.health : e->player.health;
            if (health <= 34)
                state->kill_time = GetTime();
            break;
        }
    }
    pthread_mutex_unlock(&state->entity_mutex);
}

static Vector3 handle_input(IngameState *state, Vector3 forward, Vector3 right,
                            float *wishspeed, float *camera_height) {
    int crouch_pressed =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    state->crouching = crouch_pressed;

    float speed_mult = state->crouching ? CROUCH_SPEED_MULT : 1.0f;
    *camera_height =
        state->crouching ? CROUCH_CAMERA_HEIGHT : NORMAL_CAMERA_HEIGHT;
    *wishspeed = MAX_SPEED * speed_mult;

    Vector3 wishdir = {0};

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
        if (IsKeyPressed(KEY_ENTER)) {
            state->input_state = IS_MOVING;
            uint8_t um_buf[sizeof(Packet) + sizeof(pktUserMessage)];
            Packet *um_pkt = (Packet *)um_buf;
            um_pkt->type = PKT_USER_MESSAGE;
            pktUserMessage *um_data = (pktUserMessage *)um_pkt->data;
            memcpy(um_data->message, state->message, sizeof(char[32]));

            sendto(state->sockfd, um_buf, sizeof(um_buf), 0,
                   (struct sockaddr *)&state->sv_addr, sizeof(state->sv_addr));

            state->message[0] = '\0';
            state->message_len = 0;
        }
        break;

    case IS_MOVING:
        if (IsKeyPressed(KEY_T))
            state->input_state = IS_CHAT;
        else if (IsKeyPressed(KEY_ESCAPE)) {
            state->input_state = IS_MENU;
            ShowCursor();
        }
        if (IsKeyDown(KEY_U))
            ShowCursor();
        if (IsKeyDown(KEY_I))
            DisableCursor();
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

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            shoot_weapon(state, &state->inventory[state->inventory_idx]);
        }
        break;
    case IS_MENU:
        break;
    }

    float wishlen = sqrtf(wishdir.x * wishdir.x + wishdir.z * wishdir.z);
    if (wishlen > 0) {
        wishdir.x /= wishlen;
        wishdir.z /= wishlen;
    }
    return wishdir;
}

static uint8_t move_player(IngameState *state, Vector3 wishdir, float wishspeed,
                           float frameTime) {
    state->velocity.y -= GRAVITY * frameTime;

    if (state->map) {
        float prev_vx = state->velocity.x;
        Vector3 new_pos = state->position;
        new_pos.x += state->velocity.x * frameTime;
        apply_sector_collision(&new_pos, &state->velocity, state->map,
                               STEP_HEIGHT, 0);
        if (fabsf(prev_vx) > 0.001f && state->velocity.x == 0)
            blocked_x = prev_vx > 0 ? 1.0f : -1.0f;
        else if (state->velocity.x * blocked_x < 0)
            blocked_x = 0;
        else if (blocked_x && state->velocity.x == 0 &&
                 !has_wall_in_dir(state->position, state->map,
                                  (Vector3){blocked_x, 0, 0}))
            blocked_x = 0;
        state->position.x = new_pos.x;

        float prev_vz = state->velocity.z;
        new_pos = state->position;
        new_pos.z += state->velocity.z * frameTime;
        apply_sector_collision(&new_pos, &state->velocity, state->map,
                               STEP_HEIGHT, 0);
        if (fabsf(prev_vz) > 0.001f && state->velocity.z == 0)
            blocked_z = prev_vz > 0 ? 1.0f : -1.0f;
        else if (state->velocity.z * blocked_z < 0)
            blocked_z = 0;
        else if (blocked_z && state->velocity.z == 0 &&
                 !has_wall_in_dir(state->position, state->map,
                                  (Vector3){0, 0, blocked_z}))
            blocked_z = 0;
        state->position.z = new_pos.z;

        new_pos = state->position;
        new_pos.y += state->velocity.y * frameTime;
        apply_sector_collision(&new_pos, &state->velocity, state->map,
                               STEP_HEIGHT, 1);
        state->position.y = new_pos.y;
    } else {
        blocked_x = blocked_z = 0;
        state->position = Vector3Add(state->position,
                                     Vector3Scale(state->velocity, frameTime));
    }

    if (state->position.y < 0.0f) {
        state->position.y = 0.0f;
        state->velocity.y = 0.0f;
    }

    int on_ground = state->position.y <= 0.0f ||
                    (state->map && is_on_sector_floor(state->position,
                                                      state->map, STEP_HEIGHT));

    uint8_t jump_requested = 0;
    if (IsKeyPressed(KEY_SPACE)) {
        if (on_ground) {
            state->velocity.y = JUMP_VEL;
            jump_requested = 1;
        }
    }

    if (blocked_x && wishdir.x * blocked_x > 0)
        wishdir.x = 0;
    if (blocked_z && wishdir.z * blocked_z > 0)
        wishdir.z = 0;

    if (on_ground) {
        apply_acceleration(&state->velocity, wishdir, wishspeed, GROUND_ACCEL,
                           frameTime);
        apply_ground_friction(&state->velocity, GROUND_FRICTION, frameTime);
    } else {
        apply_acceleration(&state->velocity, wishdir, wishspeed, AIR_ACCEL,
                           frameTime);
    }

    float hspeed = sqrtf(state->velocity.x * state->velocity.x +
                         state->velocity.z * state->velocity.z);
    if (hspeed > MAX_SPEED) {
        float scale = MAX_SPEED / hspeed;
        state->velocity.x *= scale;
        state->velocity.z *= scale;
    }

    return jump_requested;
}

static void disconnect_from_server(IngameState *state);

static void render_frame(Global *global, IngameState *state, float ry, float rp,
                         float camera_height) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Death cam tween
    float death_t = 0.0f;
    if (state->myself.player.health <= 0) {
        if (state->death_start == 0.0)
            state->death_start = GetTime();
        float elapsed = GetTime() - state->death_start;
        death_t = fminf(elapsed / 3.0f, 1.0f);
        camera_height *= 1.0f - death_t;
    } else {
        if (state->death_start != 0.0) {
            state->position = state->myself.position;
            state->velocity = (Vector3){0, 0, 0};
        }
        state->death_start = 0.0;
    }

    BeginDrawing();
    ClearBackground(BLUE);
    DrawFPS(10, 10);

    Camera3D camera = {0};
    camera.position =
        Vector3Add(state->position, (Vector3){0, camera_height, 0});
    camera.target = (Vector3){camera.position.x + sinf(ry) * cosf(rp),
                              camera.position.y + sinf(rp),
                              camera.position.z + cosf(ry) * cosf(rp)};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);
    if (state->map)
        draw_map(state->map, global->assets);
    pthread_mutex_lock(&state->entity_mutex);
    NetEntity snapshot[MAX_ENTITIES];
    NetEntity prev_snapshot[MAX_ENTITIES];
    int count = state->entity_count;
    memcpy(snapshot, state->entities, count * sizeof(NetEntity));
    memcpy(prev_snapshot, state->prev_entities, count * sizeof(NetEntity));
    double update_time = state->last_update_time;
    double prev_time = state->prev_update_time;
    pthread_mutex_unlock(&state->entity_mutex);

    float interp = 1.0f;
    double interval = update_time - prev_time;
    if (interval > 0.0) {
        interp = (GetTime() - update_time) / interval;
        if (interp < 0.0f) interp = 0.0f;
        if (interp > 1.0f) interp = 1.0f;
    }

    for (int i = 0; i < count; i++) {
        if (!snapshot[i].active)
            continue;
        NetEntity render_ent = snapshot[i];
        if (prev_snapshot[i].active) {
            render_ent.position = Vector3Lerp(prev_snapshot[i].position,
                                              snapshot[i].position, interp);
        }
        render_net_entity(&camera, global->assets, render_ent, global);
    }
    {
        float tick_ival = 1.0f / 20.0f;
        float frac = (GetTime() - state->last_update_time) / tick_ival;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        for (uint16_t si = 0; si < state->shot_count; si++) {
            Shot *sh = &state->shots[si];
            float age = (float)(state->server_tick - sh->spawn_tick) + frac;
            float t = age / 5.0f;
            if (t >= 1.0f) {
                DrawLine3D(sh->start, sh->end, YELLOW);
            } else if (t > 0.0f) {
                Vector3 pos = Vector3Lerp(sh->start, sh->end, t);
                DrawLine3D(sh->start, pos, YELLOW);
            }
        }
    }

    for (int i = 0; i < count; i++) {
        if (!snapshot[i].active)
            continue;
        Color box_color = snapshot[i].type == ENT_NPC_GENERIC ? RED : GREEN;
        Vector3 pos = snapshot[i].position;
        if (prev_snapshot[i].active) {
            pos = Vector3Lerp(prev_snapshot[i].position,
                              snapshot[i].position, interp);
        }
        Vector3 box_center = {pos.x, pos.y + 0.9f, pos.z};
        DrawCubeWires(box_center, 0.6f, 1.f, 0.6f, box_color);
    }
    DrawCubeWires((Vector3){state->position.x, state->position.y + 0.7f, state->position.z},
                  0.6f, 1.2f, 0.6f, GREEN);

    EndMode3D();

    if (death_t > 0.0f) {
        DrawRectangle(0, 0, sw, sh, (Color){255, 0, 0, (unsigned char)(80 * death_t)});
        const char *died = "YOU DIED";
        int died_font = 60;
        Vector2 died_size = MeasureTextEx(global->assets->default_font, died, died_font, 0);
        DrawTextEx(global->assets->default_font, died,
                   (Vector2){(sw - died_size.x) / 2.0f, (sh - died_size.y) / 2.0f - 40},
                   died_font, 0, RED);
    }

    int cross_size = 15, cross_gap = 5;
    int cross_cx = sw / 2, cross_cy = sh / 2;

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

    float hit_elapsed = GetTime() - state->hit_time;
    if (hit_elapsed < 0.15f) {
        float alpha = 1.0f - hit_elapsed / 0.15f;
        Color c = {0, 255, 0, (unsigned char)(255 * alpha)};
        int hm = 8;
        DrawLineEx((Vector2){(float)(cross_cx - hm), (float)(cross_cy - hm)},
                   (Vector2){(float)(cross_cx - hm / 2), (float)(cross_cy - hm / 2)}, 2.f, c);
        DrawLineEx((Vector2){(float)(cross_cx + hm), (float)(cross_cy - hm)},
                   (Vector2){(float)(cross_cx + hm / 2), (float)(cross_cy - hm / 2)}, 2.f, c);
        DrawLineEx((Vector2){(float)(cross_cx - hm), (float)(cross_cy + hm)},
                   (Vector2){(float)(cross_cx - hm / 2), (float)(cross_cy + hm / 2)}, 2.f, c);
        DrawLineEx((Vector2){(float)(cross_cx + hm), (float)(cross_cy + hm)},
                   (Vector2){(float)(cross_cx + hm / 2), (float)(cross_cy + hm / 2)}, 2.f, c);
    }

    Weapon *cur_weapon = &state->inventory[state->inventory_idx];
    Texture2D tex = get_texture(global->assets, cur_weapon->heldtexture);
    int tex_w = tex.width, tex_h = tex.height;
    float hspeed = sqrtf(state->velocity.x * state->velocity.x +
                         state->velocity.z * state->velocity.z);
    float speed_ratio = fminf(hspeed / MAX_SPEED, 1.0f);
    float bob_x = sinf(state->bob_phase) * BOB_AMP_X * speed_ratio;
    float bob_y = fabsf(cosf(state->bob_phase)) * BOB_AMP_Y * speed_ratio;
    Rectangle desired = (Rectangle){sw / 4 * 2 + bob_x, sh - tex_h + bob_y,
                                    tex_w, tex_h};
    DrawTexturePro(tex, (Rectangle){0, 0, tex_w, tex_h}, desired,
                   (Vector2){0, 0}, 0.f, WHITE);

    {
        char name[32];
        strcpy(name, cur_weapon->viewtexture);
        name[0] = name[0] >= 'a' && name[0] <= 'z' ? name[0] - 32 : name[0];
        char wep_info[64];
        snprintf(wep_info, sizeof(wep_info), "%s  %d", name, cur_weapon->ammo);
        Vector2 sz = MeasureTextEx(global->assets->default_font, wep_info, 18, 0);
        DrawTextEx(global->assets->default_font, wep_info,
                   (Vector2){(sw - sz.x) / 2, cross_cy + cross_size + 20},
                   18, 0, WHITE);
    }

    for (int i = 0; i < count; i++) {
        if (!snapshot[i].active || snapshot[i].type != ENT_PLAYER)
            continue;
        Vector3 pos = snapshot[i].position;
        if (prev_snapshot[i].active) {
            pos = Vector3Lerp(prev_snapshot[i].position,
                              snapshot[i].position, interp);
        }
        draw_username_billboard(camera, global->assets->default_font,
                                pos, snapshot[i].player.username);
    }

    const char *chat = state->chat;
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

    switch (state->input_state) {
    case IS_CHAT: {
        int fontSize = 22, padX = 10, padY = 8, lineH = 26, maxLines = 5;
        int boxW = sw / 2, boxX = 20;
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
            DrawTextEx(
                global->assets->default_font, line,
                (Vector2){boxX + padX, boxY + padY + (fc - 1 - i) * lineH}, 20,
                0, WHITE);
        }

        int inputY = boxY + chatAreaH + 4;
        DrawRectangle(boxX, inputY, boxW, inputH, (Color){30, 30, 30, 220});
        DrawRectangleLines(boxX, inputY, boxW, inputH,
                           (Color){180, 180, 180, 160});

        char preview[32];
        snprintf(preview, sizeof(preview), "gael: %s", state->message);
        DrawTextEx(global->assets->default_font, preview,
                   (Vector2){boxX + padX, inputY + padY}, fontSize, 0, WHITE);
        break;
    }
    case IS_MOVING:
        for (int i = found - 1; i >= 0; i--) {
            char line[128];
            const char *end = strchr(chat_lines[i], '\n');
            int len =
                end ? (int)(end - chat_lines[i]) : (int)strlen(chat_lines[i]);
            snprintf(line, sizeof(line), "%.*s", len, chat_lines[i]);
            DrawTextEx(global->assets->default_font, line,
                       (Vector2){20, sh - 120 + (found - 1 - i) * 26}, 15, 0,
                       (Color){255, 255, 255, 255});
        }

        break;
    case IS_MENU: {
        DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 200});
        int cx = sw / 2;
        int col_x = sw / 4;
        int title_size = 40;
        Vector2 title = MeasureTextEx(global->assets->default_font, "PAUSED",
                                       title_size, 0);
        DrawTextEx(global->assets->default_font, "PAUSED",
                   (Vector2){(sw - title.x) / 2, 30}, title_size, 0, WHITE);

        int player_count = 0;
        for (int i = 0; i < count; i++) {
            if (snapshot[i].active && snapshot[i].type == ENT_PLAYER)
                player_count++;
        }
        char header[32];
        snprintf(header, sizeof(header), "Players (%d):", player_count);
        int list_y = 90;
        DrawTextEx(global->assets->default_font, header,
                   (Vector2){col_x, list_y}, 22, 0, LIGHTGRAY);

        int entry_y = list_y + 30;
        for (int i = 0; i < count; i++) {
            if (!snapshot[i].active || snapshot[i].type != ENT_PLAYER)
                continue;
            char line[64];
            snprintf(line, sizeof(line), "%s  HP: %d",
                     snapshot[i].player.username,
                     snapshot[i].player.health);
            DrawTextEx(global->assets->default_font, line,
                       (Vector2){col_x + 10, entry_y}, 18, 0, WHITE);
            entry_y += 24;
        }

        int btn_w = 160, btn_h = 36;
        int btn_x = cx - btn_w / 2;
        int btn_y = sh - 120;
        if (GuiButton((Rectangle){btn_x, btn_y, btn_w, btn_h}, "Resume")) {
            state->input_state = IS_MOVING;
            DisableCursor();
        }
        if (GuiButton((Rectangle){btn_x, btn_y + btn_h + 10, btn_w, btn_h},
                       "Disconnect")) {
            disconnect_from_server(state);
            global->gamemode = GM_MENU;
            memset(state, 0, sizeof(IngameState));
        }
        break;
    }
    }
    float kill_elapsed = GetTime() - state->kill_time;
    if (kill_elapsed < 0.5f) {
        float alpha = 200.0f * (1.0f - kill_elapsed / 0.5f);
        DrawRectangle(0, 0, sw, sh, (Color){255, 255, 255, (unsigned char)alpha});
    }

    EndDrawing();
}

static void send_player_update(IngameState *state, uint8_t jump_requested) {
    uint8_t uu_buf[sizeof(Packet) + sizeof(pktUserUpdate)];
    Packet *uu_pkt = (Packet *)uu_buf;
    uu_pkt->type = PKT_USER_UPDATE;
    state->updatePkt.position = state->position;
    state->updatePkt.current_velocity = state->velocity;
    state->updatePkt.jump_requested = jump_requested;
    memcpy(uu_pkt->data, &state->updatePkt, sizeof(state->updatePkt));
    sendto(state->sockfd, uu_buf, sizeof(uu_buf), 0,
           (struct sockaddr *)&state->sv_addr, sizeof(state->sv_addr));
    state->updatePkt.shot_count = 0;
    state->updatePkt.sound_count = 0;
}

static void disconnect_from_server(IngameState *state) {
    uint8_t buf[sizeof(Packet)];
    Packet *pkt = (Packet *)buf;
    pkt->type = PKT_USER_DISCONNECT;
    sendto(state->sockfd, buf, sizeof(buf), 0,
           (struct sockaddr *)&state->sv_addr, sizeof(state->sv_addr));
    close(state->sockfd);
}

void game_loop(Global *global) {
    IngameState *state = &global->ingame;

    float frameTime = GetFrameTime();
    if (frameTime > 0.25f)
        frameTime = 0.25f;

    float ry, rp;
    Vector3 forward, right;
    handle_camera(state, &ry, &rp, &forward, &right);

    float wishspeed = 0, camera_height = NORMAL_CAMERA_HEIGHT;
    Vector3 wishdir = {0};
    uint8_t jump_requested = 0;

    if (state->myself.player.health > 0) {
        wishdir = handle_input(state, forward, right, &wishspeed, &camera_height);
        jump_requested = move_player(state, wishdir, wishspeed, frameTime);
    }

    {
        float hspeed = sqrtf(state->velocity.x * state->velocity.x +
                             state->velocity.z * state->velocity.z);
        int on_ground = state->position.y <= 0.0f ||
                        (state->map && is_on_sector_floor(state->position,
                                                          state->map, STEP_HEIGHT));
        if (on_ground && hspeed > 0.1f) {
            state->bob_phase += hspeed * BOB_SPEED * frameTime;
            while (state->bob_phase - state->last_step_phase >= PI) {
                state->last_step_phase += PI;
                Sound s = get_sound(global->assets, "footstep");
                if (s.stream.buffer)
                    PlaySound(s);
            }
        } else {
            state->last_step_phase = state->bob_phase;
        }
    }

    render_frame(global, state, ry, rp, camera_height);

    {
        pthread_mutex_lock(&state->entity_mutex);
        int sc = state->recv_sound_count;
        SoundWorld sw_buf[16];
        if (sc > 16) sc = 16;
        memcpy(sw_buf, state->recv_sounds, sc * sizeof(SoundWorld));
        state->recv_sound_count = 0;
        pthread_mutex_unlock(&state->entity_mutex);

        for (int i = 0; i < sc; i++) {
            float dist = Vector3Distance(sw_buf[i].position, state->position);
            float volume = fmaxf(0.0f, 1.0f - dist / 100.0f);
            volume = volume * volume;
            if (volume > 0.01f) {
                Sound s = get_sound(global->assets, sw_buf[i].sound_name);
                if (s.stream.buffer) {
                    SetSoundVolume(s, volume);
                    PlaySound(s);
                }
            }
        }
    }

    if (state->damage_taken) {
        state->damage_taken = 0;
        const char *snd = GetRandomValue(0, 1) ? "headshot1" : "headshot2";
        Sound s = get_sound(global->assets, snd);
        if (s.stream.buffer)
            PlaySound(s);
    }

    if (state->myself.player.health > 0)
        send_player_update(state, jump_requested);
}
