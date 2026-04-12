#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"

typedef enum EntityType {
    ENT_PLAYER,
    ENT_LIGHT,
    ENT_NPC_GENERIC,
    ENT_PROP
} EntityType;

typedef struct PlayerData {
    char username[16];
    int health;
    float yaw, pitch;
} PlayerData;

typedef struct NpcData {
    int health;
} NpcData;

typedef struct PropData {
    int model_id; // Better than storing the full Model struct here
} PropData;

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

#endif
