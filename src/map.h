#ifndef MAP_H
#define MAP_H

#define MAX_BOXES 1024

#include "raylib.h"

typedef struct Box {
    Vector3 position;
    Vector3 size;
    char texture_name[20];
} Box;

typedef struct Map {
    Box boxes;
} Map;

#endif // !MAP_H
