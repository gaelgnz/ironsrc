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
} Map;

Map *load_map(const char *file_path);
#endif // !MAP_H
