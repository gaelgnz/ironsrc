#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "entity.h"
#include "raylib.h"
#include "stdint.h"
#include <stddef.h>

#define NOT_CLIENT -1

typedef enum PacketType {
    PKT_USER_UPDATE,
    PKT_USER_JOIN,
    PKT_USER_JOIN_ACK,
    PKT_USER_DISCONNECT,
    PKT_SERVER_UPDATE,

} packetType;

typedef struct {
    uint8_t type;
    uint8_t data[];
} Packet;

typedef struct pktUserUpdate {
    Vector3 position;
    Vector3 current_velocity;
} pktUserUpdate;

typedef struct pktUserJoin {
    char userName[12];
} pktUserJoin;

typedef struct pktUserJoinAck {
    bool accepted;
} pktUserJoinAck;

typedef struct pktServerUpdate {
    NetEntity entities[256];
    int entity_count;
    uint32_t tick;
} pktServerUpdate;

inline void *pack_packet_typed(void *buf, int type, const void *payload,
                               size_t size);

#endif
