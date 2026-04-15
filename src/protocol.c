#include "protocol.h"
#include "string.h"
void *pack_packet_typed(void *buf, int type, const void *payload, size_t size) {
    Packet *pkt = (Packet *)buf;
    pkt->type = type;
    memcpy(pkt->data, payload, size);
    return buf;
}
