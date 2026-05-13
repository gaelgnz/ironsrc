/*
entity.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef ENTITY_H
#define ENTITY_H

#define NOT_PLAYER -1
#define MAX_ENTITIES 256
#include "raylib.h"

typedef enum EntityType {
    ENT_PLAYER,
    ENT_PLAYER_START,
    ENT_LIGHT,
    ENT_NPC_GENERIC,
    ENT_PROP,
} EntityType;

typedef struct PlayerData {
    char username[12];
    int health;
} PlayerData;
// Example, add and disable in your game
typedef struct NpcData {
    int health;
    char texture[32];
} NpcData;

typedef struct Enemy {
    char texture[32];
    int health;
} Enemy;

typedef struct PropData {
    int model_id;
} PropData;

typedef struct NetEntity {
    EntityType type;
    Vector3 position;
    Vector3 velocity;
    bool active;
    union {
        PlayerData player;
        NpcData npc;
        PropData prop;
    };
} NetEntity;

typedef struct Entity {
    int id;
    int client_id; // -1 = npc 0+ = player
    EntityType type;
    Vector3 position;
    Vector3 velocity;
    bool active;

    union {
        PlayerData player;
        NpcData npc;
        PropData prop;
    };
} Entity;

typedef struct Server Server;
Vector3 entity_pathfind(Vector3 initial_pos, struct Server *server);

#endif
