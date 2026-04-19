#ifndef MAP_H
#define MAP_H

#define MAX_BOXES 1024

#include "entity.h"
#include "raylib.h"

typedef struct {
    Vector3 p1, p2, p3; // 3 points defining the plane
    char texture[64];
    // float offset_x, offset_y;
    // float rotation;
    // float scale_x, scale_y;
} BrushFace;

typedef struct {
    BrushFace faces[64];
    int face_count;
} Brush;

typedef struct Map {
    Brush worldspawn[5];
    int brush_count;
    Entity entities[256];
    int entity_count;
} Map;

Map *load_map(const char *file_path);
#endif // !MAP_H
