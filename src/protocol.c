/*
protocol.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "protocol.h"
#include "string.h"
void *pack_packet_typed(void *buf, int type, const void *payload, size_t size) {
    Packet *pkt = (Packet *)buf;
    pkt->type = type;
    memcpy(pkt->data, payload, size);
    return buf;
}
