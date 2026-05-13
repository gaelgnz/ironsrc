/*
entity.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "entity.h"
#include "map.h"
#include "raymath.h"
#include "server.h"

Vector3 entity_pathfind(Vector3 pos, Server *server) {
    Entity *target = NULL;
    float best = INFINITY;
    for (int i = 0; i < server->entity_count; i++) {
        Entity *e = &server->entities[i];
        if (!e->active || e->type != ENT_PLAYER) continue;
        float dx = e->position.x - pos.x;
        float dz = e->position.z - pos.z;
        float d = dx*dx + dz*dz;
        if (d < best) { best = d; target = e; }
    }
    if (!target) return pos;
    return target->position;
}
