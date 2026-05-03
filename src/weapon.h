#ifndef WEAPON_H
#define WEAPON_H

#include "assets.h"
#include <raylib.h>
#include <stdbool.h>
typedef enum WeaponType {
    WT_RAYCAST,
    WT_PROJECTILE,
} WeaponType;

typedef struct Weapon {
    char viewtexture[64];
    char heldtexture[64];
    WeaponType type;
    float firerate;
    bool automatic;
} Weapon;

#endif // !WEAPON_H
