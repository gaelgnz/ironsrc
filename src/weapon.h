/*
weapon.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
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
    char viewtexture[32];
    char heldtexture[32];
    char shottexture[32];
    WeaponType type;
    float firerate;
    bool automatic;
} Weapon;

typedef struct Shot {
    Vector3 start;
    Vector3 end;
    int damage;
} Shot;

Weapon revolver();

#endif // !WEAPON_H
