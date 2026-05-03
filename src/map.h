#ifndef MAP_H
#define MAP_H

#define MAX_SECTORS 1024

#include "entity.h"
#include "raylib.h"
typedef struct Sector {
    char texture[32];
    int height;
    int ceiling;
} Sector;
typedef struct Map {
    Sector sectors[MAX_SECTORS];
} Map;

Map *load_map(const char *file_path);
#endif // !MAP_H
