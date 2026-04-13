#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "entity.h"
#include "raylib.h"
#include <stdint.h>

#define NOT_CLIENT -1

typedef struct {
    uint8_t type;
    uint8_t data[];
} Packet;

typedef struct UserCmd {
    Vector3 position;
    Vector3 current_velocity;
    bool jumping;
    bool attacking;
    Vector2 mouseDelta;
} UserCmd;

typedef struct pktUserJoin {
    char userName[12];
} pktUserJoin;

typedef struct pktUserJoinAck {
    bool accepted;
} pktUserJoinAck;

typedef struct pktServerUpdate {
    Entity entities[256];
    int entity_count;
    uint32_t tick;

} pktServerUpdate;
#endif
