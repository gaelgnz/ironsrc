/*
map.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef MAP_H
#define MAP_H

#define MAX_SECTORS 1024

#include "entity.h"
#include "raylib.h"
typedef struct Sector {
    char texture[32];
    int floor_height;
    int ceiling_height;
    bool ceiling_enabled;
    // 2D top-down position and size (x/z in 3D)
    int x, y;
    int width, height;
} Sector;
typedef struct Map {
    Sector sectors[MAX_SECTORS];
    int sector_count;

    Entity entities[MAX_ENTITIES];
    int entity_count;
} Map;

Map *load_map(const char *file_path);

Sector *get_sector_at(Map *map, Vector3 pos);
int is_on_sector_floor(Vector3 position, Map *map, float step_height);
void apply_sector_collision(Vector3 *position, Vector3 *velocity, Map *map,
                            float step_height);

#endif // !MAP_H
