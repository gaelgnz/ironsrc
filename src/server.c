#include "server.h"
#include "entity.h"
#include "raymath.h"
#include "stdio.h"
#include "string.h"
#include <raylib.h>

void sv_receive_input(Server *server, int client_id, UserCmd cmd) {
    if (server->last_commands[client_id].jumping)
        cmd.jumping = true; // keep it latched
    server->last_commands[client_id] = cmd;
}

void sv_spawn_entity(Server *server, Entity entity) {
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
            UserCmd cmd = server->last_commands[e->client_id];

            float accel = 50.0f; // How fast you reach max speed
            float sensitivity = 0.1f;

            e->player.yaw -= (cmd.mouseDelta.x * sensitivity);
            e->player.pitch -= (cmd.mouseDelta.y * sensitivity);

            if (e->player.pitch > 89.0f)
                e->player.pitch = 89.0f;
            if (e->player.pitch < -89.0f)
                e->player.pitch = -89.0f;

            float radYaw = e->player.yaw * DEG2RAD;
            Vector3 forward = {sinf(radYaw), 0, cosf(radYaw)};
            Vector3 right = {cosf(radYaw), 0, -sinf(radYaw)};

            // ACCELERATE: Add to velocity instead of replacing it
            e->velocity.x += (forward.x * cmd.wishVelocity.z +
                              right.x * cmd.wishVelocity.x) *
                             accel * dt;
            e->velocity.z += (forward.z * cmd.wishVelocity.z +
                              right.z * cmd.wishVelocity.x) *
                             accel * dt;

            if (cmd.jumping && e->position.y <= 0.01f) {
                e->velocity.y = 7.0f;
                server->last_commands[e->client_id].jumping =
                    false; // consume it
            }
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
}
void sv_init(Server *server) {
    server->entity_count = 1;
    strncpy(server->entities[0].player.username, "gael",
            sizeof(server->entities[0].player.username) - 1);
    server->entities[0].type = ENT_PLAYER;
    server->entities[0].client_id = 0;
    server->entities[0].position = (Vector3){0, 0, 0};
    server->entities[0].active = true;

    Entity entity = (Entity){0};
    entity.position = Vector3One();
    entity.active = true;
    entity.type = ENT_NPC_GENERIC;
    sv_spawn_entity(server, entity);
}
