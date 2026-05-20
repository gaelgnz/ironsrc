/*
protocol.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "entity.h"
#include "map.h"
#include "raylib.h"
#include "weapon.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_MSG_LEN 32
#define NOT_CLIENT -1
#define MAX_USERNAME_LEN 12
#define MAX_SHOTS 1024
#define MAX_SOUNDS 64
typedef enum PacketType {
    PKT_USER_UPDATE,
    PKT_USER_JOIN,
    PKT_USER_JOIN_ACK,
    PKT_USER_DISCONNECT,
    PKT_SERVER_UPDATE,
    PKT_USER_MESSAGE,
} packetType;

typedef struct {
    uint8_t type;
    uint8_t data[];
} Packet;

typedef struct SoundWorld {
    Vector3 position;
    char sound_name[32];
} SoundWorld;

typedef struct pktUserUpdate {
    Vector3 position;
    Vector3 current_velocity;
    uint8_t jump_requested;
    Shot shots[16];
    uint8_t shot_count;

    SoundWorld sounds[5];
    uint8_t sound_count;
} pktUserUpdate;

typedef struct pktUserJoin {
    char username[12];
} pktUserJoin;

typedef struct pktUserJoinAck {
    bool accepted;
} pktUserJoinAck;

typedef struct pktServerUpdate {
    NetEntity entities[256];

    char chat[8192];

    Entity your_player;
    int entity_count;
    uint32_t tick;

    Shot shots[MAX_SHOTS];
    uint16_t shot_count;

    SoundWorld sounds[MAX_SOUNDS];
    uint16_t sound_count;
} pktServerUpdate;

typedef struct pktUserMessage {
    char message[MAX_MSG_LEN];
} pktUserMessage;

void *pack_packet_typed(void *buf, int type, const void *payload, size_t size);

#endif
