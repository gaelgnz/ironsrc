#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "raylib.h"

// This is what the Client sends to the Server
typedef struct UserCmd {
    Vector3 wishVelocity; // Directional input
    bool jumping;
    bool attacking;
    Vector2 mouseDelta; // For camera rotation
} UserCmd;

#endif
