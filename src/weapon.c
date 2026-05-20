/*
weapon.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/

#include "weapon.h"

Weapon revolver() {
    return (Weapon){
        "gun", "gun", "gun", 16, WT_RAYCAST, 1.f, false,
    };
}

Weapon crowbar() {
    return (Weapon){
        "crowbar", "crowbar", "crowbar", 0, WT_MELEE, 0.5, 0
    };
}
